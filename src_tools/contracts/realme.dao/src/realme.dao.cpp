#include <realme.dao/realme.dao.hpp>

#include <math.hpp>
#include <utils.hpp>
#include <realme.owner.hpp>

static constexpr eosio::name active_permission{"active"_n};
static constexpr symbol   APL_SYMBOL          = symbol(symbol_code("APL"), 4);
static constexpr eosio::name MT_BANK{"amax.token"_n};

#define ALLOT_APPLE(farm_contract, lease_id, to, quantity, memo) \
    {   aplink::farm::allot_action(farm_contract, { {_self, active_perm} }).send( \
            lease_id, to, quantity, memo );}

namespace amax {

using namespace std;

#define CHECKC(exp, code, msg) \
   { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ")  \
                                    + string("[[") + _self.to_string() + string("]] ") + msg); }


   inline int64_t get_precision(const symbol &s) {
      int64_t digit = s.precision();
      CHECK(digit >= 0 && digit <= 18, "precision digit " + std::to_string(digit) + " should be in range[0,18]");
      return calc_precision(digit);
   }

   void realme_dao::init( const uint8_t& recover_threshold_pct, 
                           const name realme_owner_contract) {
      require_auth( _self );
      _gstate.recover_threshold_pct       = recover_threshold_pct;
      _gstate.realme_owner_contract         = realme_owner_contract;
   }

   void realme_dao::newaccount(const name& auth_contract, const name& creator, const name& account,  const authority& active ) {
      // check(is_account(account), "account invalid: " + account.to_string());
      recover_auth_t recoverauth(account);
      CHECKC( !_dbc.get(recoverauth), err::RECORD_EXISTING, "realme account already created" )
      auto now           = current_time_point();

      auto auditconf = _audit_item(auth_contract);
      CHECKC( auditconf.account_actived, err::NO_AUTH , "No permission to create account :" + auth_contract.to_string());

      // bool required = _audit_item(auth_contract);
      recoverauth.account 		                              = account;
      recoverauth.auth_requirements[auth_contract]          = auditconf.check_required;
      recoverauth.recover_threshold                         = 1;
      recoverauth.created_at                                = now;
      recoverauth.updated_at                                = now;
      _dbc.set(recoverauth, _self);

      realme_owner::newaccount_action newaccount_act(_gstate.realme_owner_contract, { {get_self(), "active"_n} });
      newaccount_act.send( auth_contract, creator, account, active);  

   }

   void realme_dao::addauth( const name& account, const name& contract ) {
      CHECKC( has_auth(account) , err::NO_AUTH, "no auth for operate" )
      _audit_item(contract);

      auto register_auth_itr     = register_auth_t(contract);
      CHECKC( !_dbc.get(account.value, register_auth_itr),  err::RECORD_EXISTING, "register auth already existed. ");

      recover_auth_t recoverauth(account);
      if ( _dbc.get(recoverauth) ) {
         CHECKC(recoverauth.auth_requirements.count(contract) == 0, err::RECORD_EXISTING, "contract already existed") 
      }

      auto register_auth_new    = register_auth_t(contract);
      register_auth_new.created_at = current_time_point();
      _dbc.set(account.value, register_auth_new, false);
   }

   void realme_dao::checkauth( const name& auth_contract, const name& account ) {
      require_auth ( auth_contract ); 
      uint8_t score         = 0;
      bool required         = _audit_item(auth_contract).check_required;

      auto register_auth_itr     = register_auth_t(auth_contract);
      CHECKC( _dbc.get(account.value, register_auth_itr),  err::RECORD_NOT_FOUND, "register auth not exist. ");
      _dbc.del_scope(account.value, register_auth_itr);

      recover_auth_t recoverauth(account);
      
      if (_dbc.get(recoverauth)) {
         CHECKC( !recoverauth.auth_requirements.count(auth_contract), err::RECORD_EXISTING, "contract not found:" +auth_contract.to_string() )
         auto count = recoverauth.auth_requirements.size();
         auto threshold = _get_threshold(count + 1, _gstate.recover_threshold_pct);
         if( recoverauth.recover_threshold <  threshold) recoverauth.recover_threshold = threshold;
         recoverauth.auth_requirements[auth_contract]       = required;
         recoverauth.updated_at                             = current_time_point();
         _dbc.set( recoverauth, _self);
      } else {
         recoverauth.auth_requirements[auth_contract]          = required;
         recoverauth.recover_threshold                         = 1;
         recoverauth.created_at                                = current_time_point();
         recoverauth.updated_at                                = current_time_point();
         _dbc.set(recoverauth, _self);
      }
   }
   
   uint32_t realme_dao::_get_threshold(uint32_t count, uint32_t pct) {
      int64_t tmp = 10 * count * pct / 100;
      return (tmp + 9) / 10;
   }

   void realme_dao::createorder(
                        const uint64_t&            sn,
                        const name&                auth_contract,
                        const name&                account,
                        const bool&                manual_check_required,
                        const uint8_t&             score,
                        const recover_target_type& recover_target) {

      require_auth(auth_contract);
      
      _audit_item(auth_contract);

      recover_auth_t::idx_t recoverauths(_self, _self.value);
      auto audit_ptr     = recoverauths.find(account.value);
      CHECKC( audit_ptr != recoverauths.end(), err::RECORD_NOT_FOUND, "account not exist. ");
      map<name, int8_t> scores;
      for ( auto& [key, value]: audit_ptr->auth_requirements ) {
         if (value) scores[key] = -1;
      }
   
      auto duration_second    = order_expiry_duration;
      if (manual_check_required) {
         audit_conf_t::idx_t auditconfs(_self, _self.value);
         auto auditconf_itx = auditconfs.get_index<"audittype"_n>();
         auto auditconf_itr =  auditconf_itx.find(RealmeCheckType::MANUAL.value);
         CHECKC( auditconf_itr != auditconf_itx.end(), err::RECORD_NOT_FOUND,
                           "record not existed, " + RealmeCheckType::MANUAL.to_string());
         duration_second    = manual_order_expiry_duration;
         scores[auditconf_itr->contract] = -1;
      }

      scores[auth_contract] = 1;
      
      recover_order_t::idx_t orders( _self, _self.value );
      auto account_index 			      = orders.get_index<"accountidx"_n>();
      auto order_itr 			         = account_index.find( account.value );
      CHECKC( order_itr == account_index.end(), err::RECORD_EXISTING, "order already existed. ");

      auto sn_index                    = orders.get_index<"snidx"_n>();
      auto sn_itr                      = sn_index.find( sn );
      CHECKC( sn_itr == sn_index.end(), err::RECORD_EXISTING, "sn already existed. ");

      _gstate.last_order_id ++;
      auto order_id           = _gstate.last_order_id; 
      auto now                = current_time_point();

      orders.emplace( _self, [&]( auto& row ) {
         row.id 					      = order_id;
         row.sn                     =  sn;
         row.account 			      = account;
         row.scores                 = scores;
         row.recover_type           = UpdateActionType::PUBKEY;
         row.recover_target         = recover_target;
         row.pay_status             = PayStatus::NOPAY;
         row.created_at             = now;
         row.expired_at             = now + eosio::seconds(duration_second);
         row.status                 = OrderStatus::PENDING;
      });
   
   }


   void realme_dao::setscore( const name& auth_contract, const name& account, const uint64_t& order_id, const uint8_t& score) {
      require_auth(auth_contract);

      recover_auth_t::idx_t recoverauths(_self, _self.value);
      auto audit_ptr     = recoverauths.find(account.value);
      CHECKC( audit_ptr != recoverauths.end(), err::RECORD_NOT_FOUND,"account not exist. ");
      CHECKC(audit_ptr->auth_requirements.count(auth_contract) > 0 , err::NO_AUTH, "no auth for set score: " + account.to_string());

      _audit_item(auth_contract);
      
      recover_order_t::idx_t orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not found. ");
      CHECKC(order_ptr->account == account , err::PARAM_ERROR, "account error: "+ account.to_string() )

      CHECKC(order_ptr->expired_at > current_time_point(), err::TIME_EXPIRED,"order already time expired")
      auto end_score = (score == 0? 0 : 1);
      orders.modify(*order_ptr, _self, [&]( auto& row ) {
         row.scores[auth_contract]     = end_score;
         row.updated_at                = current_time_point();
      });
   }

   void realme_dao::closeorder( const name& submitter, const uint64_t& order_id) {
      CHECKC( has_auth(submitter) , err::NO_AUTH, "realme_dao no auth for operate" )

      recover_order_t::idx_t orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not found. "); 
      CHECKC(order_ptr->expired_at > current_time_point(), err::TIME_EXPIRED, "order already time expired")

      audit_conf_t::idx_t auditscores(_self, _self.value);
      auto auditscore_idx = auditscores.get_index<"audittype"_n>();
      auto auditscore_itr =  auditscore_idx.find(RealmeCheckType::MANUAL.value);

      auto total_score = 0;
      for (auto& [key, value]: order_ptr->scores) {
         CHECKC(value > 0 , err::NEED_REQUIRED_CHECK, "required check: " + key.to_string())
         if( auditscore_itr == auditscore_idx.end() || auditscore_itr->contract != key ) {
            total_score += value; 
         }
      }

      recover_auth_t::idx_t recoverauths(_self, _self.value);
      auto audit_ptr     = recoverauths.find(order_ptr->account.value);
      CHECKC( audit_ptr != recoverauths.end(), err::RECORD_NOT_FOUND, "recover auth not exist. ");
      CHECKC( total_score >= audit_ptr->recover_threshold, err::SCORE_NOT_ENOUGH, "score not enough" );

      recoverauths.modify( *audit_ptr, _self, [&]( auto& row ) {
         row.last_recovered_at  = current_time_point();
      });

      _update_auth(order_ptr->account, std::get<eosio::public_key>(order_ptr->recover_target));
      orders.erase(order_ptr);
   }

   void realme_dao::delorder( const name& submitter, const uint64_t& order_id) {
      CHECKC( has_auth(submitter) , err::NO_AUTH, "no auth for operate" )

      recover_order_t::idx_t orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not found. "); 

      CHECKC(order_ptr->expired_at < current_time_point(), err::STATUS_ERROR, "order has not expired")
      orders.erase(order_ptr);
   }

   void realme_dao::addauditconf( const name& check_contract, const name& audit_type, const audit_conf_s& conf ) {
      CHECKC(has_auth(_self),  err::NO_AUTH, "no auth for operate"); 

      CHECKC(  audit_type == RealmeCheckType::MOBILENO || 
               audit_type == RealmeCheckType::SAFETYANSWER ||
               audit_type == RealmeCheckType::DID ||
               audit_type == RealmeCheckType::TELEGRAM ||
               audit_type == RealmeCheckType::FACEBOOK ||
               audit_type == RealmeCheckType::FACEBOOK ||
               audit_type == RealmeCheckType::WHATSAPP ||
               audit_type == RealmeCheckType::MAIL ||
               audit_type == RealmeCheckType::MANUAL , err::PARAM_ERROR, "audit type error: " + audit_type.to_string())

      CHECKC( conf.status == ContractStatus::RUNNING || conf.status == ContractStatus::STOPPED, 
                     err::PARAM_ERROR, "contract status error " + conf.status.to_string() )

      CHECKC( conf.max_score > 0 , err::PARAM_ERROR, "score error ")

      CHECKC(is_account(check_contract), err::PARAM_ERROR,"check_contract invalid: " + check_contract.to_string());

      audit_conf_t::idx_t auditscores(_self, _self.value);
      auto auditscore_ptr     = auditscores.find(check_contract.value);
      if( auditscore_ptr != auditscores.end() ) {
         auditscores.modify(*auditscore_ptr, _self, [&]( auto& row ) {
            row.audit_type    = audit_type;
            row.charge        = conf.charge;
            if( conf.title.length() > 0 )       row.title         = conf.title;
            if( conf.desc.length() > 0 )        row.desc          = conf.desc;
            if( conf.url.length() > 0 )         row.url           = conf.url;
            if( conf.max_score > 0 )            row.max_score   = conf.max_score;
            row.check_required = conf.check_required;
            row.status        = conf.status;
            row.account_actived = conf.account_actived;
         });   
      } else {
         auditscores.emplace(_self, [&]( auto& row ) {
            row.contract      = check_contract;
            row.audit_type    = audit_type;
            row.charge        = conf.charge;
            row.title         = conf.title;
            row.desc          = conf.desc;
            row.url           = conf.url;
            row.max_score     = conf.max_score;
            row.check_required = conf.check_required;
            row.status        = conf.status;
            row.account_actived = conf.account_actived;
         });
      }
   }

   void realme_dao::delauditconf(  const name& contract_name ) {
      CHECKC(has_auth(_self),  err::NO_AUTH, "no auth for operate");      

      audit_conf_t::idx_t auditscores(_self, _self.value);
      auto auditscore_ptr     = auditscores.find(contract_name.value);

      CHECKC( auditscore_ptr != auditscores.end(), err::RECORD_NOT_FOUND, "auditscore not exist. ");
      auditscores.erase(auditscore_ptr);
   }

   const audit_conf_t& realme_dao::_audit_item(const name& contract) {
      audit_conf_t::idx_t auditscores(_self, _self.value);

      auto auditscore_ptr     = auditscores.find(contract.value);
      CHECKC( auditscore_ptr != auditscores.end(), err::RECORD_NOT_FOUND,  "audit_conf_t contract not exist:  " + contract.to_string());
      CHECKC( auditscore_ptr->status == ContractStatus::RUNNING, err::STATUS_ERROR,  "contract status is error: " + contract.to_string());
      return auditscores.get(contract.value);
      // return auditscore_ptr;
   }

   void realme_dao::_update_auth( const name& account, const eosio::public_key& pubkey ) {
      realme_owner::updateauth_action updateauth_act(_gstate.realme_owner_contract, { {get_self(), "active"_n} });
      updateauth_act.send( account, pubkey);   
   }

}//namespace amax