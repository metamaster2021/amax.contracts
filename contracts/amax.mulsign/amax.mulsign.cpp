#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <string>

#include "mulsign_db.hpp"
#include "amax.token.hpp"


namespace amax {


#define CHECKC(exp, code, msg) \
   { if (!(exp)) eosio::check(false, string("$$$") + to_string((int)code) + string("$$$ ") + msg); }

#define COLLECTFEE(from, to, quantity) \
    {	mulsign::collectfee_action act{ _self, { {_self, active_perm} } };\
			act.send( from, to, quantity );}

using std::string;
using namespace eosio;
using namespace wasm::db;

enum class err: uint8_t {
   NONE                 = 0,
   RECORD_NOT_FOUND     = 1,
   RECORD_EXISTING      = 2,
   SYMBOL_MISMATCH      = 4,
   PARAM_ERROR          = 5,
   PAUSED               = 6,
   NO_AUTH              = 7,
   NOT_POSITIVE         = 8,
   NOT_STARTED          = 9,
   OVERSIZED            = 10,
   TIME_EXPIRED         = 11,
   NOTIFY_UNRELATED     = 12,
   ACTION_REDUNDANT     = 13,
   ACCOUNT_INVALID      = 14,
   FEE_INSUFFICIENT     = 15,

};

class [[eosio::contract("amax.mulsign")]] mulsign : public contract {
private:
   dbc                 _db;
   global_singleton    _global;
   global_t            _gstate;

public:
   using contract::contract;

   mulsign(eosio::name receiver, eosio::name code, datastream<const char*> ds):
      _db(_self), contract(receiver, code, ds), _global(_self, _self.value) {
      if (_global.exists()) {
         _gstate = _global.get();

      } else { // first init
         _gstate = global_t{};
         _gstate.admin = _self;
      }
   }

   ~mulsign() { _global.set( _gstate, get_self() ); }

   ACTION init() {
      require_auth( _self );

      _gstate.fee_collector = "amax.daodev"_n;
      _gstate.wallet_fee = asset_from_string("0.10000000 AMAX");

      // CHECKC(false, err::NONE, "init disallowed!")

      // auto proposals = proposal_t::idx_t(_self, _self.value);
      // auto itr = proposals.begin();
      // proposals.erase(itr);

      // auto wallets = wallet_t::idx_t(_self, _self.value);
      // auto itr = wallets.begin();
      // wallets.erase(itr);
   }

   // /**
   //  * @brief create a multisign wallet, returns a unqiued wallet_id
   //  * 
   //  * @param issuer 
   //  * @param mulsign_m 
   //  * @param mulsign_n 
   //  * @return * create, 
   //  */
   // ACTION createmsign(const name& issuer, const uint8_t& mulsign_m, const uint8_t& mulsign_n) {
   //    require_auth( issuer );

   //    auto mwallets = wallet_t::idx_t(_self, _self.value);
   //    auto wallet_id = mwallets.available_primary_key(); if (wallet_id == 0) wallet_id = 1;  
   //    auto wallet = wallet_t(wallet_id);
   //    wallet.mulsign_m = mulsign_m;
   //    wallet.mulsign_n = mulsign_n;
   //    wallet.creator = issuer;
   //    wallet.created_at = time_point_sec(current_time_point());
   //    _db.set( wallet, issuer );
   // }

   /**
    * @brief add a mulsinger into a target wallet, must add all mulsigners within 24 hours upon creation
    * @param issuer
    * @param wallet_id
    * @param mulsigner
    * 
    */
   ACTION setmulsigner(const name& issuer, const uint64_t& wallet_id, const name& mulsigner, const uint8_t& weight) {
      require_auth( issuer );

      auto wallet = wallet_t(wallet_id);
      CHECKC( _db.get(wallet), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(wallet_id) )
      CHECKC( wallet.creator == issuer, err::NO_AUTH, "only creator allowed to add cosinger: " + wallet.creator.to_string() )
      int64_t elapsed =  current_time_point().sec_since_epoch() - wallet.created_at.sec_since_epoch();
      CHECKC( elapsed < seconds_per_day, err::TIME_EXPIRED, "setmulsigner exceeded 24-hour time window" )
      CHECKC( is_account(mulsigner), err::ACCOUNT_INVALID, "invlid mulsigner: " + mulsigner.to_string() )
      CHECKC( wallet.mulsigners.size() < wallet.mulsign_n, err::OVERSIZED, "wallet.mulsign_n has been reached: " + to_string(wallet.mulsign_n) );
      
      wallet.mulsigners[mulsigner] = weight;
      wallet.updated_at = time_point_sec( current_time_point() );
      _db.set( wallet, issuer );

   }

   /**
    * @brief set proposal expiry time in seconds for a given wallet
    * @param issuer - wallet owner only
    * @param wallet_id 
    * @param expiry_sec - expiry time in seconds for wallet proposals
    * 
    */
   ACTION setwapexpiry(const name& issuer, const uint64_t wallet_id, const uint64_t& expiry_sec) {
      require_auth( issuer );

      auto wallet = wallet_t(wallet_id);
      CHECKC( _db.get(wallet), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(wallet_id) )
      CHECKC( wallet.creator == issuer, err::NO_AUTH, "only creator allowed to add cosinger: " + wallet.creator.to_string() )

      wallet.proposal_expiry_sec = expiry_sec;
      _db.set( wallet, issuer );
   }

   /**
    * @brief only an existing mulsign can remove him/herself from the mulsingers list
    *        or wallet owner can remove it within 24 hours
    * @param issuer
    * @param wallet_id
    * @param mulsigner
    * 
    */
   ACTION delmulsigner(const name& issuer, const uint64_t& wallet_id, const name& mulsigner) {
      require_auth( issuer );

      auto wallet = wallet_t(wallet_id);
      CHECKC( _db.get( wallet ), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(wallet_id) )
      CHECKC( issuer == mulsigner || issuer == wallet.creator, err::NO_AUTH, "unauthorized" )

      if (issuer == wallet.creator) {
         CHECKC( issuer != mulsigner, err::NO_AUTH, "owner can't remove self wallet" )

         auto elapsed = current_time_point().sec_since_epoch() - wallet.created_at.sec_since_epoch();
         CHECKC( elapsed < seconds_per_day, err::TIME_EXPIRED, "owner to delmulsigner exceeded 24-hour time window")
      }

      wallet.mulsigners.erase(mulsigner);
      wallet.updated_at = time_point_sec( current_time_point() );
      _db.set( wallet, issuer );
   
   }

   /**
    * @brief lock amount into mulsign wallet, memo: $bank:$walletid
    * 
    * @param from
    * @param to
    * @param quantity
    * @param memo: 1) create:$m:$n:$title; 2) lock:$wallet_id
    */
   [[eosio::on_notify("*::transfer")]]
   void ontransfer(const name& from, const name& to, const asset& quantity, const string& memo) {
      CHECKC( from != to, err::ACCOUNT_INVALID,"cannot transfer to self" );
      CHECKC( quantity.amount > 0, err::PARAM_ERROR, "non-positive quantity not allowed" )
      CHECKC( memo != "", err::PARAM_ERROR, "empty memo!" )

      auto bank_contract = get_first_receiver();

      vector<string_view> memo_params = split(memo, ":");
      if (memo_params[0] == "create" && memo_params.size() == 4) {
         uint64_t m = stoi(string(memo_params[1]));
         uint64_t n = stoi(string(memo_params[2]));
         string title = string(memo_params[3]);
         CHECKC( title.length() < 1024, err::OVERSIZED, "wallet title too long" )
         CHECKC( bank_contract == SYS_BANK && quantity.symbol == SYS_SYMBOL, err::PARAM_ERROR, "non-sys-symbol" )
         CHECKC( quantity >= _gstate.wallet_fee, err::FEE_INSUFFICIENT, "insufficient wallet fee: " + quantity.to_string() )

         if (from != _gstate.fee_collector)
            COLLECTFEE( from, _gstate.fee_collector, quantity )

         // lock_funds(0, bank_contract, quantity);
         create_wallet(from, m, n, title);

      } else if (memo_params[0] == "lock" && memo_params.size() == 2) {
         auto wallet_id = (uint64_t) stoi(string(memo_params[1]));
         
         lock_funds(wallet_id, bank_contract, quantity);

      } else {
         CHECKC(false, err::PARAM_ERROR, "invalid memo" )
      }
   }

   /**
    * @brief fee collect action
    * 
    */
   ACTION collectfee(const name& from, const name& to, const asset& quantity) {
      require_auth( _self );
      require_recipient( _gstate.fee_collector );
   }
   using collectfee_action = eosio::action_wrapper<"collectfee"_n, &mulsign::collectfee>;

   /**
    * @brief anyone can propose to withdraw asset from a pariticular wallet
    * 
    * @param issuer 
    * @param wallet_id 
    * @param quantity 
    * @param to 
    * @return * anyone* 
    */
   ACTION propose(const name& issuer, const uint64_t& wallet_id, const extended_asset& ex_asset, const name& recipient, const string& excerpt, const string& meta_url) {
      require_auth( issuer );
      
      auto wallet = wallet_t(wallet_id);
      auto ext_symb_str = ext_to_string(ex_asset);
      CHECKC( _db.get( wallet ), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(wallet_id) )
      CHECKC( wallet.assets.find(ext_symb_str) != wallet.assets.end(), err::PARAM_ERROR, "withdraw symbol err: " + ext_symb_str )

      auto avail_quant = wallet.assets[ ext_symb_str ];
      CHECKC( ex_asset.quantity.amount <= avail_quant, err::OVERSIZED, "overdrawn proposal: " + ex_asset.quantity.to_string() + " > " + to_string(avail_quant) )

      CHECKC( excerpt.length() < 1024, err::OVERSIZED, "excerpt length >= 1024" )
      CHECKC( meta_url.length() < 2048, err::OVERSIZED, "meta_url length >= 2048" )

      auto proposals = proposal_t::idx_t(_self, _self.value);
      auto pid = proposals.available_primary_key();
      auto proposal = proposal_t(pid);
      proposal.wallet_id = wallet_id;
      proposal.quantity = ex_asset;
      proposal.recipient = recipient;
      proposal.proposer = issuer;
      proposal.excerpt = excerpt;
      proposal.meta_url = meta_url;
      proposal.created_at = time_point_sec(current_time_point());
      proposal.expired_at = time_point_sec(proposal.created_at + wallet.proposal_expiry_sec);

      _db.set(proposal, issuer);
   }

/**
 * @brief cancel a proposal before it expires
 * 
 */
ACTION cancel(const name& issuer, const uint64_t& proposal_id) {
   require_auth( issuer );

   auto proposal = proposal_t(proposal_id);
   CHECKC( _db.get( proposal ), err::RECORD_NOT_FOUND, "proposal not found: " + to_string(proposal_id) )
   CHECKC( proposal.proposer == issuer, err::NO_AUTH, "issuer is not proposer" )
   CHECKC( proposal.approvers.size() == 0, err::NO_AUTH, "proposal already appoved" )
   CHECKC( proposal.expired_at > current_time_point(), err::NO_AUTH, "proposal already expired" )
      
   _db.del( proposal );
}

/**
 * @brief only mulsigner can approve the proposal: the m-th of n mulsigner will trigger its execution
 * @param issuer
 * @param  
 */
ACTION approve(const name& issuer, const uint64_t& proposal_id, const bool approved) {
   require_auth( issuer );

   auto proposal = proposal_t(proposal_id);
   CHECKC( _db.get( proposal ), err::RECORD_NOT_FOUND, "proposal not found: " + to_string(proposal_id) )
   CHECKC( proposal.executed_at == time_point_sec(), err::ACTION_REDUNDANT, "proposal already executed: " + to_string(proposal_id) )
   CHECKC( proposal.expired_at >= current_time_point(), err::TIME_EXPIRED, "the proposal already expired" )
   CHECKC( !proposal.approvers.count(issuer), err::ACTION_REDUNDANT, "issuer (" + issuer.to_string() +") already approved" )

   auto wallet = wallet_t(proposal.wallet_id);
   CHECKC( _db.get( wallet ), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(proposal.wallet_id) )
   CHECKC( wallet.mulsigners.count(issuer), err::NO_AUTH, "issuer (" + issuer.to_string() +") not allowed to approve" )
   
   proposal.approvers.insert(issuer);
   proposal.recv_votes += wallet.mulsigners[issuer]; 

   _db.set(proposal, issuer);
}

ACTION execute(const name& issuer, const uint64_t& proposal_id) {
   require_auth( issuer );

   auto proposal = proposal_t(proposal_id);
   CHECKC( _db.get( proposal ), err::RECORD_NOT_FOUND, "proposal not found: " + to_string(proposal_id) )
   CHECKC( proposal.executed_at != time_point_sec(), err::ACTION_REDUNDANT, "proposal already executed: " + to_string(proposal_id) )
   CHECKC( proposal.expired_at >= current_time_point(), err::TIME_EXPIRED, "the proposal already expired" )
   
   auto wallet = wallet_t(proposal.wallet_id);
   CHECKC( _db.get( wallet ), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(proposal.wallet_id) )
   CHECKC( proposal.recv_votes >= wallet.mulsign_m, err::NO_AUTH, "insufficient votes" )

   execute_proposal(wallet, proposal);
   _db.set(proposal);
}

private:

   void create_wallet(const name& creator, const uint64_t& m, const uint64_t& n, const string& title) {
      auto mwallets = wallet_t::idx_t(_self, _self.value);
      auto wallet_id = mwallets.available_primary_key(); 
      if (wallet_id == 0 && creator != _gstate.fee_collector)
         wallet_id = 1;  //starts from 1

      auto wallet = wallet_t(wallet_id);
      wallet.title = title;
      wallet.mulsign_m = m;
      wallet.mulsign_n = n;
      wallet.creator = creator;
      wallet.created_at = time_point_sec(current_time_point());

      _db.set( wallet, _self );

   }

   inline string ext_to_string(const extended_asset& ext_asset) {
      return ext_asset.get_extended_symbol().get_symbol().code().to_string() + "@" +  ext_asset.contract.to_string();
   }

   inline string ext_to_string(const name& bank_contract, const symbol& symb) {
      return symb.code().to_string() + "@" +  bank_contract.to_string();
   }

   void lock_funds(const uint64_t& wallet_id, const name& bank_contract, const asset& quantity) {
      auto wallet = wallet_t(wallet_id);
      CHECKC( _db.get( wallet ), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(wallet_id) )

      auto ext_symb_str = ext_to_string(bank_contract, quantity.symbol);
      if ( wallet.assets.find(ext_symb_str) != wallet.assets.end() ) {
         wallet.assets[ ext_symb_str ] = quantity.amount;

      } else {
         auto curr_amount = wallet.assets[ ext_symb_str ];
         wallet.assets[ ext_symb_str ] = curr_amount + quantity.amount;
      }

      _db.set( wallet, _self );
   }

   void execute_proposal(wallet_t& wallet, proposal_t &proposal) {   
      auto ext_symb_str = ext_to_string(proposal.quantity);
      auto avail_quant = wallet.assets[ ext_symb_str ];
      CHECKC( proposal.quantity.quantity.amount <= avail_quant, err::OVERSIZED, "Overdrawn not allowed: " + proposal.quantity.quantity.to_string() + " > " + to_string(avail_quant) );
      
      wallet.assets[ ext_symb_str ] -= proposal.quantity.quantity.amount;
      _db.set(wallet);

      auto asset_bank = proposal.quantity.contract;
      TRANSFER( asset_bank, proposal.recipient, proposal.quantity.quantity, "mulsign execute" )

      proposal.executed_at = time_point_sec( current_time_point() );
   }

};
}
