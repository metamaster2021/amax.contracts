#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <string>

#include "amax.mulsign.hpp"

using namespace amax;

ACTION mulsign::init(const name& fee_collector, const asset& wallet_fee) {
   require_auth( _self );

   CHECKC( wallet_fee.symbol == SYS_SYMBOL, err::SYMBOL_MISMATCH, "require fee type: AMAX");
   CHECKC( wallet_fee.amount > 0, err::NOT_POSITIVE, "fee must be positive");
   CHECKC( is_account(fee_collector), err::ACCOUNT_INVALID, "invalid fee collector: " + fee_collector.to_string() )
   CHECKC( _gstate.fee_collector == name(), err::RECORD_EXISTING, "cannot init twice");
   
   _gstate.fee_collector = fee_collector;
   _gstate.wallet_fee = wallet_fee;
   _create_wallet(fee_collector, "amax.daodev", 10);
}

ACTION mulsign::setfee(const uint64_t& wallet_id, const asset& wallet_fee){
   require_auth(get_self());
   CHECKC(wallet_id == 0, err::NO_AUTH, "no auth to change fee")
   CHECKC(wallet_fee.symbol == SYS_SYMBOL, err::SYMBOL_MISMATCH, "only support fee as AMAX")
   CHECKC(wallet_fee.amount > 0, err::NOT_POSITIVE, "fee must be a positive value")

   _gstate.wallet_fee = wallet_fee;
}

ACTION mulsign::setmulsigner(const name& issuer, const uint64_t& wallet_id, const name& mulsigner, const uint32_t& weight) {
   require_auth( issuer );
   CHECKC( is_account(mulsigner), err::ACCOUNT_INVALID, "invalid mulsigner: " + mulsigner.to_string() )
   CHECKC( weight>0 && weight<=100, err::NOT_POSITIVE, "weight must be a number between 1~100")

   auto wallet = wallet_t(wallet_id);
   CHECKC( _db.get(wallet), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(wallet_id) )
   int64_t elapsed =  current_time_point().sec_since_epoch() - wallet.created_at.sec_since_epoch();
   CHECKC((wallet.creator == issuer && elapsed < seconds_per_day) || issuer == get_self(), err::NO_AUTH, "only creator or propose proposal to add cosigner" )
   
   //notify user when add
   if( wallet.mulsigners.count(mulsigner) == 0 ) require_recipient(mulsigner);
   wallet.mulsigners[mulsigner] = weight;
   CHECKC(wallet.mulsigners.size() <= MAX_MULSIGNER_LENGTH, err::OVERSIZED, "max number of mulsigners is " + to_string(MAX_MULSIGNER_LENGTH));
   uint32_t total_weight = 0;
   for (const auto& item : wallet.mulsigners) {
      total_weight += item.second;
   }
   CHECKC( total_weight >= wallet.mulsign_m, err::OVERSIZED, "total weight " + to_string(wallet.mulsign_n) + "must be greater than  m: " + to_string(wallet.mulsign_m) );
   wallet.mulsign_n = total_weight;
   wallet.updated_at = current_time_point();
   _db.set( wallet, issuer );
}

ACTION mulsign::setmulsignm(const name& issuer, const uint64_t& wallet_id, const uint32_t& mulsignm){
   require_auth( issuer );

   CHECKC( mulsignm > 0, err::PARAM_ERROR, "m must be a positive num");
   auto wallet = wallet_t(wallet_id);
   CHECKC( _db.get(wallet), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(wallet_id) )
   int64_t elapsed =  current_time_point().sec_since_epoch() - wallet.created_at.sec_since_epoch();
   CHECKC( (wallet.creator == issuer && elapsed < seconds_per_day) || issuer == get_self(), err::NO_AUTH, "only creator or proposal allowed to edit m")
   auto total_weight = wallet.mulsign_n;
   CHECKC( mulsignm <= total_weight, err::OVERSIZED, "total weight " + to_string(total_weight) + "cannot be less than  m: " + to_string(mulsignm));
   
   wallet.mulsign_m = mulsignm;
   wallet.updated_at = current_time_point();
   _db.set( wallet, issuer );
}

ACTION mulsign::setproexpiry(const name& issuer, const uint64_t& wallet_id, const uint64_t& expiry_sec) {
   require_auth( issuer );

   auto wallet = wallet_t(wallet_id);
   CHECKC( _db.get(wallet), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(wallet_id) )
   int64_t elapsed =  current_time_point().sec_since_epoch() - wallet.created_at.sec_since_epoch();
   CHECKC( (wallet.creator == issuer && elapsed < seconds_per_day) || issuer == get_self(), err::NO_AUTH, "only creator or proposal allowed to set expiry")
   CHECKC( expiry_sec >= seconds_per_day && expiry_sec <= 30 * seconds_per_day, 
            err::PARAM_ERROR, "suggest expiry_sec is 1~30 days");

   wallet.proposal_expiry_sec = expiry_sec;
   wallet.updated_at = current_time_point();
   _db.set( wallet, issuer );
}

ACTION mulsign::delmulsigner(const name& issuer, const uint64_t& wallet_id, const name& mulsigner) {
   require_auth( issuer );

   auto wallet = wallet_t(wallet_id);
   CHECKC( _db.get( wallet ), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(wallet_id) )
   int64_t elapsed = current_time_point().sec_since_epoch() - wallet.created_at.sec_since_epoch();
   CHECKC( (wallet.creator == issuer && elapsed < seconds_per_day) || issuer == get_self(), err::NO_AUTH, "only creator or proposal allowed to del cosigner")
   CHECKC( wallet.mulsigners.count(mulsigner)==1, err::RECORD_NOT_FOUND, "cannot found mulsigner: "+mulsigner.to_string());

   wallet.mulsigners.erase(mulsigner);
   uint32_t total_weight = 0;
   for (const auto& item : wallet.mulsigners) {
      total_weight += item.second;
   }
   CHECKC( total_weight >= wallet.mulsign_m, err::OVERSIZED, "total weight " + to_string(wallet.mulsign_n) + "must be greater than  m: " + to_string(wallet.mulsign_m) );
   wallet.mulsign_n = total_weight;
   wallet.updated_at = current_time_point();
   _db.set( wallet, issuer );
   require_recipient(mulsigner);
}

void mulsign::ontransfer(const name& from, const name& to, const asset& quantity, const string& memo) {
   if(from == get_self() || to != get_self()) return;
   CHECKC( from != to, err::ACCOUNT_INVALID,"cannot transfer to self" );
   CHECKC( quantity.amount > 0, err::PARAM_ERROR, "non-positive quantity not allowed" )
   CHECKC( memo != "", err::PARAM_ERROR, "empty memo!" )

   auto bank_contract = get_first_receiver();

   vector<string_view> memo_params = split(memo, ":");
   if (memo_params[0] == "create" && memo_params.size() == 3) {
      string title = string(memo_params[1]);
      uint32_t wight = to_uint32(memo_params[2], "Weight param error");
      CHECKC( title.length() < 32, err::OVERSIZED, "wallet title too long, suggest length below 32" )
      CHECKC( bank_contract == SYS_BANK && quantity.symbol == SYS_SYMBOL, err::PARAM_ERROR, "non-sys-symbol" )
      CHECKC( quantity >= _gstate.wallet_fee, err::FEE_INSUFFICIENT, "insufficient wallet fee: " + quantity.to_string() )

      COLLECTFEE( from, _gstate.fee_collector, quantity )

      _create_wallet(from, title, wight);
      _lock_funds(0, bank_contract, quantity);

   } else if (memo_params[0] == "lock" && memo_params.size() == 2) {
      auto wallet_id = (uint64_t) stoi(string(memo_params[1]));
      _lock_funds(wallet_id, bank_contract, quantity);

   } else {
      CHECKC(false, err::PARAM_ERROR, "invalid memo" )
   }
}

ACTION mulsign::collectfee(const name& from, const name& to, const asset& quantity) {
   require_auth( _self );
   require_recipient( _gstate.fee_collector );
}

ACTION mulsign::proposeact(const name& issuer, 
                   const uint64_t& wallet_id, 
                   const action& execution, 
                   const string& excerpt, 
                   const string& description,
                   const uint32_t& duration){
      propose(issuer, wallet_id, execution.name, execution.account, execution.data, excerpt, description, duration);
                   }

ACTION mulsign::propose(const name& issuer, 
                   const uint64_t& wallet_id, 
                   const name& action_name, 
                   const name& action_account, 
                   const std::vector<char>& packed_action_data, 
                   const string& excerpt, 
                   const string& description,
                   const uint32_t& duration) {
   require_auth( issuer );

   auto wallet = wallet_t(wallet_id);
   CHECKC( _db.get( wallet ), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(wallet_id) )
   CHECKC( wallet.mulsigners.count(issuer), err::ACCOUNT_INVALID, "only mulsigner can propose actions" )
   CHECKC( duration>0, err::NOT_POSITIVE, "duration must be a positive number" )
   CHECKC( wallet.proposal_expiry_sec >= duration && duration >= 0, err::OVERSIZED, "duration should be less than expiry_sec" )
   const auto expiry = duration ==0? wallet.proposal_expiry_sec: duration;

   CHECKC( is_account(action_account), err::ACCOUNT_INVALID, "contract invalid: " + action_account.to_string())
   if (action_name != proposal_type::transfer) 
      CHECKC( action_account == get_self(), err::NO_AUTH, "no auth to submit proposal to other account")
   
   auto proposals = proposal_t::idx_t(_self, _self.value);
   auto pid = proposals.available_primary_key();
   auto proposal = proposal_t(pid);
   permission_level pem({_self, "active"_n});
   switch (action_name.value)
   {
      case proposal_type::setfee.value: {
         setfee_data action_data = unpack<setfee_data>(packed_action_data); 
         proposal.execution = action(pem, action_account, action_name, action_data);
         break;
      }
      case proposal_type::transfer.value: {
         transfer_data action_data = unpack<transfer_data>(packed_action_data);
         proposal.execution = action(pem, action_account, action_name, action_data);
         break;
      }
      case proposal_type::setmulsignm.value: {
         setmulsignm_data action_data = unpack<setmulsignm_data>(packed_action_data);
         proposal.execution = action(pem, action_account, action_name, action_data);
         break;
      }
      case proposal_type::setmulsigner.value: {
         setmulsigner_data action_data = unpack<setmulsigner_data>(packed_action_data);
         proposal.execution = action(pem, action_account, action_name, action_data);
         break;
      }
      case proposal_type::delmulsigner.value: {
         delmulsigner_data action_data = unpack<delmulsigner_data>(packed_action_data);   
         proposal.execution = action(pem, action_account, action_name, action_data);
         break;
      }
      case proposal_type::setproexpiry.value: {
         setproexpiry_data action_data = unpack<setproexpiry_data>(packed_action_data); 
         proposal.execution = action(pem, action_account, action_name, action_data);
         break;
      }
      default: {
         CHECKC( false, err::PARAM_ERROR, "Unsupport proposal type")
         break;
      }
   }

   _check_proposal_params(wallet, proposal.execution);
   
   CHECKC( excerpt.length() <= MAX_TITLE_LENGTH, err::OVERSIZED, "excerpt length <= 64" )
   CHECKC( description.length() <= MAX_CONTENT_LENGTH, err::OVERSIZED, "description length <= 512" )
   
   proposal.wallet_id = wallet_id;
   proposal.proposer = issuer;
   proposal.excerpt = excerpt;
   proposal.description = description;
   proposal.status = proposal_status::PROPOSED;
   proposal.created_at = current_time_point();
   proposal.expired_at = proposal.created_at + expiry;
   _db.set(proposal, issuer);
}

ACTION mulsign::cancel(const name& issuer, const uint64_t& proposal_id) {
   require_auth( issuer );

   const auto& now = current_time_point();
   auto proposal = proposal_t(proposal_id);
   CHECKC( _db.get( proposal ), err::RECORD_NOT_FOUND, "proposal not found: " + to_string(proposal_id) )
   CHECKC( proposal.proposer == issuer, err::NO_AUTH, "issuer is not proposer" )
   CHECKC( proposal.approvers.size() == 0, err::NO_AUTH, "proposal already appoved" )
   CHECKC( proposal.expired_at > now, err::NO_AUTH, "proposal already expired" )

   proposal.updated_at = now;
   proposal.status = proposal_status::CANCELED;
   _db.set( proposal );
}

ACTION mulsign::respond(const name& issuer, const uint64_t& proposal_id, uint8_t vote) {
   require_auth( issuer );

   const auto& now = current_time_point();
   auto proposal = proposal_t(proposal_id);
   CHECKC( _db.get( proposal ), err::RECORD_NOT_FOUND, "proposal not found: " + to_string(proposal_id) )
   CHECKC( proposal.status == proposal_status::PROPOSED || proposal.status == proposal_status::APPROVED, 
      err::STATUS_ERROR, "proposal can not be approved at status: " + proposal.status.to_string() )
   CHECKC( proposal.expired_at > now, err::TIME_EXPIRED, "the proposal already expired" )
   CHECKC( !proposal.approvers.count(issuer), err::ACTION_REDUNDANT, "issuer (" + issuer.to_string() +") already approved" )

   auto wallet = wallet_t(proposal.wallet_id);
   CHECKC( _db.get( wallet ), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(proposal.wallet_id) )
   CHECKC( wallet.mulsigners.count(issuer), err::NO_AUTH, "issuer (" + issuer.to_string() +") not allowed to approve" )
   CHECKC( vote == proposal_vote::PROPOSAL_AGAINST 
      || vote == proposal_vote::PROPOSAL_FOR, err::PARAM_ERROR, "unsupport result" )

   proposal.approvers.insert(map<name,uint32_t>::value_type(issuer, vote?wallet.mulsigners[issuer]:0));
   if(vote == proposal_vote::PROPOSAL_FOR) 
      proposal.recv_votes += wallet.mulsigners[issuer];
   proposal.updated_at = now;
   proposal.status = proposal_status::APPROVED;
   _db.set(proposal, issuer);
}

ACTION mulsign::execute(const name& issuer, const uint64_t& proposal_id) {
   require_auth( issuer );
   const auto& now = current_time_point();
   auto proposal = proposal_t(proposal_id);
   CHECKC( _db.get( proposal ), err::RECORD_NOT_FOUND, "proposal not found: " + to_string(proposal_id) )
   CHECKC( proposal.status == proposal_status::APPROVED, err::STATUS_ERROR,
           "proposal can not be executed at status: " + proposal.status.to_string() )

   auto wallet = wallet_t(proposal.wallet_id);
   CHECKC( _db.get( wallet ), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(proposal.wallet_id) )
   CHECKC( proposal.recv_votes >= wallet.mulsign_m, err::NO_AUTH, "insufficient votes" )

   _execute_proposal(wallet, proposal);
   proposal.updated_at = now;
   proposal.status = proposal_status::EXECUTED;
   _db.set(proposal);
}

void mulsign::_create_wallet(const name& creator, const string& title, const uint32_t& wight) {
   auto mwallets = wallet_t::idx_t(_self, _self.value);
   auto wallet_id = mwallets.available_primary_key();
   if (wallet_id == 0) {
      CHECKC(creator == _gstate.fee_collector, err::FIRST_CREATOR, "the first creator must be fee_collector: " + _gstate.fee_collector.to_string());
   }
   CHECKC(wight >= 10, err::PARAM_ERROR, "init weight value suggest 10 or bigger")

   auto wallet = wallet_t(wallet_id);
   wallet.title = title;
   wallet.mulsign_m = wight;
   wallet.mulsign_n = wight;
   wallet.mulsigners[creator] = wight;
   wallet.creator = creator;
   wallet.created_at = current_time_point();
   wallet.proposal_expiry_sec = 7 * seconds_per_day;

   _db.set( wallet, _self );
}

void mulsign::_lock_funds(const uint64_t& wallet_id, const name& bank_contract, const asset& quantity) {
   auto wallet = wallet_t(wallet_id);
   CHECKC( _db.get( wallet ), err::RECORD_NOT_FOUND, "wallet not found: " + to_string(wallet_id) )

   const auto& symb = extended_symbol(quantity.symbol, bank_contract);
   wallet.assets[ symb ] += quantity.amount;
   _db.set( wallet, _self );
}

void mulsign::_check_proposal_params(const wallet_t& wallet, const action& execution){
   auto packed_action_data = execution.data;
   auto action_account = execution.account;
   auto action_name = execution.name;

   switch (action_name.value)
   {
      case proposal_type::setfee.value: {
         setfee_data action_data = unpack<setfee_data>(packed_action_data);
         CHECKC( action_data.wallet_id == wallet.id, err::NO_AUTH, "no auth to submit proposal for other wallet")
         CHECKC(action_data.wallet_id == 0, err::NO_AUTH, "no auth to change fee")
         CHECKC(action_data.wallet_fee.symbol == SYS_SYMBOL, err::SYMBOL_MISMATCH, "only support fee as AMAX")
         CHECKC(action_data.wallet_fee.amount > 0, err::NOT_POSITIVE, "fee must be a positive value")
         break;
      }
      case proposal_type::transfer.value: {
         transfer_data action_data = unpack<transfer_data>(packed_action_data);
         CHECKC( action_data.to != get_self(), err::ACCOUNT_INVALID, "cannot transfer to self")
         CHECKC( is_account(action_data.to), err::ACCOUNT_INVALID, "account invalid: " + action_data.to.to_string());
         CHECKC( action_data.memo.length() <= MAX_CONTENT_LENGTH, err::OVERSIZED, "max memo length <= 512" )

         auto ex_asset = extended_asset(action_data.quantity, action_account);
         const auto& symb = ex_asset.get_extended_symbol();
         CHECKC( wallet.assets.count(symb), err::PARAM_ERROR,
            "symbol not found in wallet: " + ex_asset.quantity.to_string() + "@" + ex_asset.contract.to_string() )
         CHECKC( ex_asset.quantity.amount > 0, err::PARAM_ERROR, "withdraw quantity must be positive" )
         auto avail_quant = wallet.assets.at(symb);
         CHECKC( ex_asset.quantity.amount <= avail_quant, err::OVERSIZED, "overdrawn proposal: " + ex_asset.quantity.to_string() + " > " + to_string(avail_quant) )
         break;
      }
      case proposal_type::setmulsignm.value: {
         setmulsignm_data action_data = unpack<setmulsignm_data>(packed_action_data);
         CHECKC( action_data.wallet_id == wallet.id, err::NO_AUTH, "no auth to submit proposal for other wallet")
         CHECKC( action_data.mulsignm > 0, err::NOT_POSITIVE, "mulsignm must be a positive number")
         CHECKC( action_data.mulsignm <= wallet.mulsign_n, err::OVERSIZED, "total weight oversize than m: " + to_string(wallet.mulsign_m) )
         break;
      }
      case proposal_type::setmulsigner.value: {
         setmulsigner_data action_data = unpack<setmulsigner_data>(packed_action_data);
         CHECKC( action_data.wallet_id == wallet.id, err::NO_AUTH, "no auth to submit proposal for other wallet")
         CHECKC( is_account(action_data.mulsigner), err::ACCOUNT_INVALID, "account invalid: " + action_data.mulsigner.to_string());
         CHECKC( action_data.weight>0 && action_data.weight<=100, err::NOT_POSITIVE, "weight must be a number between 1~100")
         break;
      }
      case proposal_type::delmulsigner.value: {
         delmulsigner_data action_data = unpack<delmulsigner_data>(packed_action_data);   
         CHECKC( action_data.wallet_id == wallet.id, err::NO_AUTH, "no auth to submit proposal for other wallet") 
         CHECKC( is_account(action_data.mulsigner), err::ACCOUNT_INVALID, "account invalid: " + action_data.mulsigner.to_string());
         CHECKC( wallet.mulsigners.count(action_data.mulsigner), err::ACCOUNT_INVALID, "account not in mulsigners: " + action_data.mulsigner.to_string());
         break;
      }
      case proposal_type::setproexpiry.value: {
         setproexpiry_data action_data = unpack<setproexpiry_data>(packed_action_data);   
         CHECKC( action_data.wallet_id == wallet.id, err::NO_AUTH, "no auth to submit proposal for other wallet") 
         CHECKC( action_data.expiry_sec >= seconds_per_day && action_data.expiry_sec <= 30 * seconds_per_day, 
            err::PARAM_ERROR, "suggest expiry_sec is 1~30 days");
         break;
      }
      default: {
         CHECKC( false, err::PARAM_ERROR, "Unsupport proposal type")
         break;
      }
   }
}

void mulsign::_execute_proposal(wallet_t& wallet, proposal_t &proposal) {
   _check_proposal_params(wallet, proposal.execution);

   if(proposal.execution.name == proposal_type::transfer){
      transfer_data action_data = unpack<transfer_data>(proposal.execution.data);

      const auto& symb = extended_symbol( action_data.quantity.symbol, proposal.execution.account);
      auto avail_quant = wallet.assets[ symb ];
      CHECKC(  action_data.quantity.amount <= avail_quant, err::OVERSIZED, "Overdrawn not allowed: " +  action_data.quantity.to_string() + " > " + to_string(avail_quant) );

      if ( action_data.quantity.amount == avail_quant) {
         wallet.assets.erase(symb);
      } else {
         wallet.assets[ symb ] -=  action_data.quantity.amount;
      }
      _db.set(wallet);
   }
   proposal.execution.send();
}
