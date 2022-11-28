#include <amax.recover/amax.recover.hpp>

#include<math.hpp>

#include <utils.hpp>
#include <amax_proxy.hpp>

static constexpr eosio::name active_permission{"active"_n};
static constexpr symbol   APL_SYMBOL          = symbol(symbol_code("APL"), 4);
static constexpr eosio::name MT_BANK{"amax.token"_n};

#define ALLOT_APPLE(farm_contract, lease_id, to, quantity, memo) \
    {   aplink::farm::allot_action(farm_contract, { {_self, active_perm} }).send( \
            lease_id, to, quantity, memo );}

namespace amax {

using namespace std;

#define CHECKC(exp, code, msg) \
   { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ") + msg); }


   inline int64_t get_precision(const symbol &s) {
      int64_t digit = s.precision();
      CHECK(digit >= 0 && digit <= 18, "precision digit " + std::to_string(digit) + " should be in range[0,18]");
      return calc_precision(digit);
   }

   void amax_recover::init( const uint8_t& score_limit, 
                           const name amax_proxy_contract) {
      require_auth( _self );
      _gstate.score_limit                 = score_limit;
      _gstate.amax_proxy_contract         = amax_proxy_contract;
   }

   void amax_recover::bindaccount( const name& account, const name& default_checker ) {
      require_auth ( _gstate.amax_proxy_contract );
      check(is_account(account), "account invalid: " + account.to_string());
      account_audit_t::idx accountaudits(_self, _self.value);
      auto audit_ptr     = accountaudits.find(account.value);
      CHECKC( audit_ptr == accountaudits.end(), err::RECORD_EXISTING, "account already exist. ");
      auto now           = current_time_point();

      uint8_t score = 0;
      bool required = _get_audit_item(default_checker, score);

      accountaudits.emplace( _self, [&]( auto& row ) {
         row.audit_contracts[default_checker]   = required ? ContractAuditStatus::REQUIRED : ContractAuditStatus::OPTIONAL;
         row.account 		                     = account;
         row.threshold                          = _gstate.score_limit;
         row.created_at                         = now;
      });   
   }

   void amax_recover::addauth( const name& account, const name& contract ) {
      CHECKC( has_auth(account) , err::NO_AUTH, "no auth for operate" )
      uint8_t score = 0;
      _get_audit_item(contract, score);

      account_audit_t::idx account_audits(_self, _self.value);
      auto audit_ptr     = account_audits.find(account.value);
      CHECKC( audit_ptr != account_audits.end(), err::RECORD_NOT_FOUND, "account not exist. ");
      auto now           = current_time_point();

      CHECKC(audit_ptr->audit_contracts.count(contract) == 0, err::RECORD_EXISTING, "contract already existed") 

      account_audits.modify( *audit_ptr, _self, [&]( auto& row ) {
         row.audit_contracts[contract]  = ContractAuditStatus::REGISTED;
         row.created_at                 = now;
      });   
   }

   void amax_recover::checkauth( const name& checker_contract, const name& account ) {
      require_auth ( checker_contract ); 
      uint8_t score = 0;
      bool required = _get_audit_item(checker_contract, score);

      account_audit_t::idx account_audits(_self, _self.value);
      auto audit_ptr     = account_audits.find(account.value);
      CHECKC( audit_ptr != account_audits.end(), err::RECORD_NOT_FOUND, "account record not exist: " + account.to_string());
      auto now           = current_time_point();

      CHECKC(audit_ptr->audit_contracts.count(checker_contract) != 0, err::RECORD_NOT_FOUND, "contract not existed:" +checker_contract.to_string()  )
      
      account_audits.modify(*audit_ptr, _self, [&]( auto& row ) {
         row.audit_contracts[checker_contract]  = required ? ContractAuditStatus::REQUIRED : ContractAuditStatus::OPTIONAL ;
         row.created_at                 = now;
      });   

   }

   void amax_recover::createorder(
                        const uint64_t&            serial_num,
                        const name&                admin,
                        const name&                account,
                        const recover_target_type& recover_target,
                        const bool&                manual_check_required) {
      _check_action_auth(admin, ActionPermType::CREATEORDER);

      account_audit_t::idx accountaudits(_self, _self.value);
      auto audit_ptr     = accountaudits.find(account.value);
      CHECKC( audit_ptr != accountaudits.end(), err::RECORD_NOT_FOUND, "account not exist. ");
      map<name, uint8_t> scores;
      for ( auto& [key, value]: audit_ptr->audit_contracts ) {
         if (value == ContractAuditStatus::REQUIRED) {
            scores[key] = 0;
         }
      }
   
      auto duration_second    = order_expiry_duration;
      if (manual_check_required) {
         auditscore_t::idx_t auditscores(_self, _self.value);
         auto auditscore_idx = auditscores.get_index<"audittype"_n>();
         auto auditscore_itr =  auditscore_idx.find(AuditType::MANUAL.value);
         CHECKC( auditscore_itr != auditscore_idx.end(), err::RECORD_NOT_FOUND, "record not existed, " + AuditType::MANUAL.to_string());

         duration_second      = manual_order_expiry_duration;
         scores[auditscore_itr->contract] = 0;
      }

      recoverorder_t::idx_t orders( _self, _self.value );
      auto account_index 			      = orders.get_index<"accountidx"_n>();
      auto order_itr 			         = account_index.find( account.value );
      CHECKC( order_itr == account_index.end(), err::RECORD_EXISTING, "order already existed. ");

      _gstate.last_order_id ++;
      auto order_id           = _gstate.last_order_id; 
      auto now                = current_time_point();

      orders.emplace( _self, [&]( auto& row ) {
         row.id 					      = order_id;
         row.serial_num             = serial_num;
         row.account 			      = account;
         row.scores                 = scores;
         row.recover_type           = UpdateActionType::PUBKEY;
         row.recover_target         = recover_target;
         row.pay_status             = PayStatus::NOPAY;
         row.created_at             = now;
         row.expired_at             = now + eosio::seconds(duration_second);
      });
   
   }


   void amax_recover::createcorder(
                        const uint64_t&            serial_num,
                        const name&                checker_contract,
                        const name&                account,
                        const bool&                manual_check_required,
                        const uint8_t&             score,
                        const recover_target_type& recover_target) {

      require_auth(checker_contract);
      
      uint8_t  answer_score_limit  = 0;
      _get_audit_item(checker_contract, answer_score_limit);

      CHECKC(score <= answer_score_limit, err::PARAM_ERROR, "score peram error")

      account_audit_t::idx accountaudits(_self, _self.value);
      auto audit_ptr     = accountaudits.find(account.value);
      CHECKC( audit_ptr != accountaudits.end(), err::RECORD_NOT_FOUND, "account not exist. ");
      map<name, uint8_t> scores;
      for ( auto& [key, value]: audit_ptr->audit_contracts ) {
         if (value == ContractAuditStatus::REQUIRED) {
            scores[key] = 0;
         }
      }
   
      auto duration_second    = order_expiry_duration;
      if (manual_check_required) {
         auditscore_t::idx_t auditscores(_self, _self.value);
         auto auditscore_idx = auditscores.get_index<"audittype"_n>();
         auto auditscore_itr =  auditscore_idx.find(AuditType::MANUAL.value);
         CHECKC( auditscore_itr != auditscore_idx.end(), err::RECORD_NOT_FOUND, "record not existed, " + AuditType::MANUAL.to_string());

         duration_second      = manual_order_expiry_duration;
         scores[auditscore_itr->contract] = 0;
      }

      scores[checker_contract] = score;
      
      recoverorder_t::idx_t orders( _self, _self.value );
      auto account_index 			      = orders.get_index<"accountidx"_n>();
      auto order_itr 			         = account_index.find( account.value );
      CHECKC( order_itr == account_index.end(), err::RECORD_EXISTING, "order already existed. ");

      _gstate.last_order_id ++;
      auto order_id           = _gstate.last_order_id; 
      auto now                = current_time_point();

      orders.emplace( _self, [&]( auto& row ) {
         row.id 					      = order_id;
         row.serial_num             = serial_num;
         row.account 			      = account;
         row.scores                 = scores;
         row.recover_type           = UpdateActionType::PUBKEY;
         row.recover_target         = recover_target;
         row.pay_status             = PayStatus::NOPAY;
         row.created_at             = now;
         row.expired_at             = now + eosio::seconds(duration_second);
      });
   
   }


   void amax_recover::setscore( const name& checker_contract, const name& account, const uint64_t& order_id, const uint8_t& score) {
      require_auth(checker_contract);

      uint8_t  answer_score_limit  = 0;
      _get_audit_item(checker_contract, answer_score_limit);

      recoverorder_t::idx_t orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not found. ");
      CHECKC(answer_score_limit >= score, err::PARAM_ERROR, "scores exceed limit")
      CHECKC(order_ptr->account == account , err::PARAM_ERROR, "account error: "+ account.to_string() )

      CHECKC(order_ptr->expired_at > current_time_point(), err::TIME_EXPIRED, "order already time expired")

      orders.modify(*order_ptr, _self, [&]( auto& row ) {
         row.scores[checker_contract]    = score;
         row.updated_at                = current_time_point();
      });
   }

   void amax_recover::closeorder( const name& submitter, const uint64_t& order_id) {
      CHECKC( has_auth(submitter) , err::NO_AUTH, "amax_recover no auth for operate" )
      recoverorder_t::idx_t orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not found. "); 
      CHECKC(order_ptr->expired_at > current_time_point(), err::TIME_EXPIRED, "order already time expired")

      auto total_score = 0;
      for (auto& [key, value]: order_ptr->scores) {
          CHECKC(value !=0 , err::NEED_REQUIRED_CHECK, "required check: " + key.to_string())
         total_score += value;
      }

      CHECKC( total_score > _gstate.score_limit, err::SCORE_NOT_ENOUGH, "score not enough" );


      account_audit_t::idx accountaudits(_self, _self.value);
      auto audit_ptr     = accountaudits.find(order_ptr->account.value);
      CHECKC( audit_ptr != accountaudits.end(), err::RECORD_NOT_FOUND, "order not exist. ");

      accountaudits.modify( *audit_ptr, _self, [&]( auto& row ) {
         row.recovered_at  = current_time_point();
      });

      _update_auth(order_ptr->account, std::get<eosio::public_key>(order_ptr->recover_target));
      orders.erase(order_ptr);
   }

   void amax_recover::delorder( const name& submitter, const uint64_t& order_id) {
      CHECKC( has_auth(submitter) , err::NO_AUTH, "no auth for operate" )
      recoverorder_t::idx_t orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not found. "); 
      auto total_score = 0;
      // CHECKC(order_ptr->expired_at < current_time_point(), err::STATUS_ERROR, "order has not expired")
      orders.erase(order_ptr);
   
   }

   void amax_recover::setauditor( const name& account, const set<name>& actions ) {
      CHECKC(has_auth(_self),  err::NO_AUTH, "no auth for operate");      

      auditor_t::idx_t auditors(_self, _self.value);
      auto auditor_ptr = auditors.find(account.value);

      if( auditor_ptr != auditors.end() ) {
         auditors.modify(*auditor_ptr, _self, [&]( auto& row ) {
            row.actions      = actions;
         });   
      } else {
         auditors.emplace(_self, [&]( auto& row ) {
            row.account      = account;
            row.actions      = actions;
         });
      }
   }
      
   void amax_recover::delauditor(  const name& account ) {
      CHECKC(has_auth(_self),  err::NO_AUTH, "no auth for operate");      

      auditor_t::idx_t auditors(_self, _self.value);
      auto auditor_ptr     = auditors.find(account.value);

      CHECKC( auditor_ptr != auditors.end(), err::RECORD_EXISTING, "auditor not exist. ");
      auditors.erase(auditor_ptr);
   }

   void amax_recover::addcontract(  const name&    check_contract, 
                                 const name&    audit_type,
                                 const asset&   cost, 
                                 const string&  title, 
                                 const string&  desc, 
                                 const string&  url,
                                 const uint8_t& score,
                                 const name     status ) {
      CHECKC(has_auth(_self),  err::NO_AUTH, "no auth for operate"); 


      auditscore_t::idx_t auditscores(_self, _self.value);
      auto auditscore_ptr     = auditscores.find(check_contract.value);
      if( auditscore_ptr != auditscores.end() ) {
         auditscores.modify(*auditscore_ptr, _self, [&]( auto& row ) {
            row.audit_type    = audit_type;
            row.cost          = cost;
            row.title         = title;
            row.desc          = desc;
            row.url           = url;
            row.score         = score;
            row.status        = status;
         });   
      } else {
         auditscores.emplace(_self, [&]( auto& row ) {
            row.contract      = check_contract;
            row.audit_type    = audit_type;
            row.cost          = cost;
            row.title         = title;
            row.desc          = desc;
            row.url           = url;
            row.cost          = cost;
            row.score         = score;
            row.status        = status;
         });
      }
   }

   void amax_recover::delcontract(  const name& account ) {
      CHECKC(has_auth(_self),  err::NO_AUTH, "no auth for operate");      

      auditscore_t::idx_t auditscores(_self, _self.value);
      auto auditscore_ptr     = auditscores.find(account.value);

      CHECKC( auditscore_ptr != auditscores.end(), err::RECORD_NOT_FOUND, "auditscore not exist. ");
      auditscores.erase(auditscore_ptr);
   }

   bool amax_recover::_get_audit_item(const name& contract, uint8_t& score) {
      auditscore_t::idx_t auditscores(_self, _self.value);
      auto auditscore_ptr     = auditscores.find(contract.value);
      CHECKC( auditscore_ptr != auditscores.end(), err::RECORD_NOT_FOUND, "auditscore_t contract not exist:  " + contract.to_string());
      CHECKC( auditscore_ptr->status == ContractStatus::RUNNING, err::STATUS_ERROR, "contract status is error: " + contract.to_string());
      score = auditscore_ptr->score;
      return auditscore_ptr->required_check;
      
   }

   void amax_recover::_check_action_auth(const name& admin, const name& action_type) {
      if(has_auth(_self)) 
         return;
      auditor_t::idx_t auditors(_self, _self.value);
      auto auditor_ptr     = auditors.find(admin.value);
      CHECKC( auditor_ptr != auditors.end(), err::RECORD_NOT_FOUND, "auditor not exist. ");
      CHECKC( auditor_ptr->actions.count(action_type), err::NO_AUTH, "no auth for operate ");
      CHECKC(has_auth(admin),  err::NO_AUTH, "no auth for operate");      
   }

   void amax_recover::_update_auth(   const name& account,
                        const eosio::public_key& pubkey ) {
                           
      amax_proxy::updateauth_action updateauth_act(_gstate.amax_proxy_contract, { {get_self(), "active"_n} });
      updateauth_act.send( account, pubkey);   
   }

}//namespace amax