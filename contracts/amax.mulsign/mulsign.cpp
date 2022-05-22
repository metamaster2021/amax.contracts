#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <string>

#include "mulsign_db.hpp"
#include "amax.token.hpp"


namespace amax {


#define CHECKC(exp, code, msg) \
   { if (!(exp)) eosio::check(false, string("$$$") + to_string((int)code) + string("$$$ ") + msg); }


using std::string;
using namespace eosio;
using namespace wasm::db;

static constexpr uint64_t seconds_per_day       = 24 * 3600;

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

};

class [[eosio::contract("amax.mulsign")]] mulsign : public contract {
private:
   dbc                 _db;
   // global_singleton    _global;
   // global_t            _gstate;

public:
   using contract::contract;

   mulsign(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        _db(_self), contract(receiver, code, ds) {} //, _global(_self, _self.value) {
      //   if (_global.exists()) {
      //       _gstate = _global.get();

      //   } else { // first init
      //       _gstate = global_t{};
      //       _gstate.admin = _self;
      //   }
   // }

   // ~mulsign() { _global.set( _gstate, get_self() ); }

   // ACTION init() {
   //    require_auth( _self );

   //    CHECKC(false, err::NONE, "init disabled!")

   // }

   /**
    * @brief create a multisign wallet, returns a unqiued wallet_id
    * 
    * @param issuer 
    * @param mulsign_m 
    * @param mulsign_n 
    * @return * create, 
    */
   ACTION createmsign(const name& issuer, const uint8_t& mulsign_m, const uint8_t& mulsign_n) {
      require_auth( issuer );

      auto mwallets = wallet_t::idx_t(_self, _self.value);
      auto wallet_id = mwallets.available_primary_key(); 
      auto wallet = wallet_t(wallet_id, mulsign_m, mulsign_n);
      wallet.mulsigners[issuer] = 1;   //default weight as 1, can be modified in future
      wallet.creator = issuer;
      wallet.created_at = time_point_sec(current_time_point());
      _db.set( wallet );
   }

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
      _db.set( wallet );

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
      _db.set( wallet );
   
   }

   /**
    * @brief lock amount into mulsign wallet, memo: $bank:$walletid
    * 
    * @param from
    * @param to
    * @param quantity
    * @param memo
    */
   [[eosio::on_notify("*::transfer")]]
   void transfer(const name& from, const name& to, const asset& quantity, const string& memo) {
      CHECKC( to == _self, err::NOTIFY_UNRELATED, "notified but not a recipient" )
      CHECKC( quantity.amount > 0, err::PARAM_ERROR, "non-positive quantity not allowed" )
      CHECKC( memo != "", err::PARAM_ERROR, "memo contains no wallet_id" )

      auto wallet_id = (uint64_t) atoi(memo.c_str());
      auto wallet = wallet_t(wallet_id);
      CHECKC( _db.get( wallet ), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(wallet_id) )

      auto bank_contract = get_first_receiver();
      auto ext_symb = extended_symbol(quantity.symbol, bank_contract);
      if ( !wallet.assets.count(ext_symb) ) {
         wallet.assets[ext_symb] = quantity.amount;

      } else {
         auto curr_amount = wallet.assets[ext_symb];
         wallet.assets[ext_symb] = curr_amount + quantity.amount;
      }

      _db.set( wallet );
   }

/**
 * @brief anyone can propose to withdraw asset from a pariticular wallet
 * 
 * @param issuer 
 * @param wallet_id 
 * @param quantity 
 * @param to 
 * @return * anyone* 
 */
ACTION propose(const name& issuer, const uint64_t& wallet_id, const extended_asset& ex_asset, const name& recipient) {
   require_auth( issuer );
   
   auto wallet = wallet_t(wallet_id);
   CHECKC( _db.get( wallet ), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(wallet_id) )
   CHECKC( wallet.assets.count(ex_asset.get_extended_symbol()), err::PARAM_ERROR, "withdraw symbol err: " + ex_asset.quantity.to_string() )

   auto avail_quant = wallet.assets[ ex_asset.get_extended_symbol() ];
   CHECKC( ex_asset.quantity.amount <= avail_quant, err::OVERSIZED, "overdrawn proposal: " + ex_asset.quantity.to_string() + " > " + to_string(avail_quant) )

   auto proposals = proposal_t::idx_t(_self, _self.value);
   auto pid = proposals.available_primary_key();
   auto proposal = proposal_t(pid);
   proposal.wallet_id = wallet_id;
   proposal.quantity = ex_asset;
   proposal.recipient = recipient;
   proposal.created_at = current_time_point();
   proposal.expired_at = proposal.created_at + seconds_per_day;

   _db.set(proposal);
}

/**
 * @brief only mulsigner can approve the proposal: the m-th of n mulsigner will trigger its execution
 * @param issuer
 * @param  
 */
ACTION approve(const name& issuer, const uint64_t& proposal_id) {
   require_auth( issuer );

   auto proposal = proposal_t(proposal_id);
   CHECKC( _db.get( proposal ), err::RECORD_NOT_FOUND, "proposal not found: " + to_string(proposal_id) )
   CHECKC( proposal.executed_at != time_point_sec(), err::ACTION_REDUNDANT, "proposal already executed: " + to_string(proposal_id) )

   auto wallet = wallet_t(proposal.wallet_id);
   CHECKC( _db.get( wallet ), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(proposal.wallet_id) )
   CHECKC( wallet.mulsigners.count(issuer), err::NO_AUTH, "issuer (" + issuer.to_string() +") not allowed to approve" )
   CHECKC( !proposal.approvers.count(issuer), err::ACTION_REDUNDANT, "issuer (" + issuer.to_string() +") already approved" )
   CHECKC( proposal.expired_at >= current_time_point(), err::TIME_EXPIRED, "the proposal already expired" )
   
   auto m = wallet.mulsign_m;
   auto appove_cnt = proposal.approvers.size();
   CHECKC( m > appove_cnt, err::ACTION_REDUNDANT, "already approved by m (" + to_string(m) + ") signers" )
   proposal.approvers.insert(issuer);
   proposal.recv_votes += wallet.mulsigners[issuer]; 

   if (proposal.recv_votes == appove_cnt)
      execute_proposal(wallet, proposal);

    _db.set(proposal);
}

private:

   void execute_proposal(wallet_t& wallet, proposal_t &proposal) {   
      auto avail_quant = wallet.assets[proposal.quantity.get_extended_symbol()];
      CHECKC( proposal.quantity.quantity.amount <= avail_quant, err::OVERSIZED, "Overdrawn not allowed: " + proposal.quantity.quantity.to_string() + " > " + to_string(avail_quant) );
     
      wallet.assets[proposal.quantity.get_extended_symbol()] -= proposal.quantity.quantity.amount;
      _db.set(wallet);

      auto asset_bank = proposal.quantity.contract;
      TRANSFER( asset_bank, proposal.recipient, proposal.quantity.quantity, "mulsign execute" )

      proposal.executed_at = time_point_sec( current_time_point() );
   }

};
}
