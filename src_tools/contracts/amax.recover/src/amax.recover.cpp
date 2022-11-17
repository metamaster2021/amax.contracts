#include <amax.recover/amax.recover.hpp>

#include<math.hpp>

#include <utils.hpp>

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

   void amax_recover::init( const uint8_t& score_limit ) {
      require_auth( _self );
      _gstate.score_limit        = score_limit;
   }

   void amax_recover::bindaccount ( const name& admin, const name& account, const checksum256& mobile_hash ) {
      _check_action_auth(admin, ActionPermType::BINDACCOUNT);

      accountaudit_t::idx accountaudits(_self, _self.value);
      auto audit_ptr     = accountaudits.find(account.value);
      CHECKC( audit_ptr == accountaudits.end(), err::RECORD_EXISTING, "account already exist. ");
      auto now                   = current_time_point();

      accountaudits.emplace( _self, [&]( auto& row ) {
         row.account 		   = account;
         row.mobile_hash      = mobile_hash;
         row.created_at       = now;
      });   
   }

   void amax_recover::bindanswer( const name& admin, const name& account, map<uint8_t, checksum256 >& answers ) {
      
      _check_action_auth(admin, ActionPermType::BINDANSWER);

      accountaudit_t::idx accountaudits(_self, _self.value);
      auto audit_ptr     = accountaudits.find(account.value);
      CHECKC( audit_ptr != accountaudits.end(), err::RECORD_NOT_FOUND, "order not exist. ");

      accountaudits.modify( *audit_ptr, _self, [&]( auto& row ) {
         row.answers          = answers;
      });   

   }

   void amax_recover::createorder(const name& admin,
                        const name& account,
                        const checksum256& mobile_hash,
                        const refrecoverinfo& recover_target,
                        const bool& manual_check_required) {
      _check_action_auth(admin, ActionPermType::CREATEORDER);

      accountaudit_t::idx accountaudits(_self, _self.value);
      auto audit_ptr     = accountaudits.find(account.value);
      CHECKC( audit_ptr != accountaudits.end(), err::RECORD_NOT_FOUND, "account not exist. ");
      CHECKC( mobile_hash == audit_ptr->mobile_hash, err::PARAM_ERROR, "mobile hash check failed" )

      recoverorder_t::idx_t orders( _self, _self.value );
      auto xinto_index 			         = orders.get_index<"accountidx"_n>();
      auto order_itr 			         = orders.find( account.value );
      CHECKC( order_itr == orders.end(), err::RECORD_EXISTING, "order already existed. ");

      auto duration_second    = order_expiry_duration;
      if (manual_check_required) {
         duration_second      = manual_order_expiry_duration;
      }

      _gstate.last_order_id ++;
      auto order_id           = _gstate.last_order_id; 
      auto now                = current_time_point();
      int8_t mobile_check_score = -1;
      _get_audit_score(AuditType::MOBILENO, mobile_check_score);

      orders.emplace( _self, [&]( auto& row ) {
         row.id 					      = order_id;
         row.account 			      = account;
         row.recover_type           = UpdateActionType::PUBKEY;
         row.recover_target         = recover_target;
         row.mobile_check_score     = mobile_check_score;
         row.manual_check_required  = manual_check_required;
         row.pay_status             = PayStatus::NOPAY;
         row.created_at             = now;
         row.expired_at             = now + eosio::seconds(duration_second);
      });
   
   }

   void amax_recover::chkanswer( const name& admin, const uint64_t& order_id, const name& account, const int8_t& score) {
      _check_action_auth(admin, ActionPermType::CHKANSWER);
      recoverorder_t::idx_t orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not found. ");
      int8_t answer_score_limit = 0;
      _get_audit_score(AuditType::ANSWER, answer_score_limit);
      CHECKC(answer_score_limit >= score, err::PARAM_ERROR, "scores exceed limit")
      CHECKC(order_ptr->expired_at > current_time_point(), err::TIME_EXPIRED, "order already time expired")

      orders.modify(*order_ptr, _self, [&]( auto& row ) {
         row.did_check_score     = score;
         row.updated_at          = current_time_point();
      });
   }

   void amax_recover::chkdid( const name& admin, const uint64_t& order_id, const name& account, const bool& passed) {
      _check_action_auth(admin, ActionPermType::CHKDID);
      
      recoverorder_t::idx_t orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not found. ");
      CHECKC(order_ptr->expired_at > current_time_point(), err::TIME_EXPIRED, "order already time expired")
      
      int8_t score = 0;
      if (passed) {
         _get_audit_score(AuditType::DID, score);
      } 

      auto now                = current_time_point();
      orders.modify(*order_ptr, _self, [&]( auto& row ) {
         row.did_check_score     =  score;
         row.updated_at          = now;
      });
   }

   void amax_recover::chkmanual( const name& admin, const uint64_t& order_id, const name& account, const bool& passed) {
      _check_action_auth(admin, ActionPermType::CHKMANUAL);

      recoverorder_t::idx_t orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not found. ");
      CHECKC(order_ptr->expired_at > current_time_point(), err::TIME_EXPIRED, "order already time expired")

      name manual_check_result       = ManualCheckStatus::FAILURE;
      if (passed) manual_check_result   = ManualCheckStatus::SUCCESS;
      auto now                = current_time_point();
      orders.modify(*order_ptr, _self, [&]( auto& row ) {
         row.manual_check_result    = manual_check_result;
         row.updated_at             = now;
      });
   }

   void amax_recover::closeorder( const name& submitter, const uint64_t& order_id) {
      CHECKC( has_auth(submitter) , err::NO_AUTH, "no auth for operate" )
      recoverorder_t::idx_t orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not found. "); 
      CHECKC(order_ptr->expired_at > current_time_point(), err::TIME_EXPIRED, "order already time expired")

      auto total_score = 0;
      if(order_ptr->mobile_check_score > 0 ) total_score += order_ptr->mobile_check_score;
      if(order_ptr->answer_check_score > 0 ) total_score += order_ptr->answer_check_score;
      if(order_ptr->did_check_score > 0 ) total_score += order_ptr->did_check_score;
      CHECKC( total_score < _gstate.score_limit, err::SCORE_NOT_ENOUGH, "score not enough" );

      if( order_ptr->manual_check_required && order_ptr->manual_check_result == ManualCheckStatus::SUCCESS ) {
         _update_authex(order_ptr->account, std::get<eosio::public_key>(order_ptr->recover_target));
         orders.erase(order_ptr);
      }
   }

   void amax_recover::delorder( const name& submitter, const uint64_t& order_id) {
      CHECKC( has_auth(submitter) , err::NO_AUTH, "no auth for operate" )
      recoverorder_t::idx_t orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not found. "); 
      auto total_score = 0;
      CHECKC(order_ptr->expired_at < current_time_point(), err::STATUS_ERROR, "order has not expired")
      orders.erase(order_ptr);
   
   }

   void amax_recover::setauditor( const name& account, const set<name>& actions ) {
      auditor_t::idx_t auditors(_self, _self.value);
      auto auditor_ptr     = auditors.find(account.value);
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
      auditor_t::idx_t auditors(_self, _self.value);
      auto auditor_ptr     = auditors.find(account.value);

      CHECKC( auditor_ptr != auditors.end(), err::RECORD_EXISTING, "auditor not exist. ");
      auditors.erase(auditor_ptr);
   }

   void amax_recover::setscore( const name& audit_type, const int8_t& score ) {
      auditscore_t::idx_t auditscores(_self, _self.value);
      auto auditscore_ptr     = auditscores.find(audit_type.value);
      if( auditscore_ptr != auditscores.end() ) {
         auditscores.modify(*auditscore_ptr, _self, [&]( auto& row ) {
            row.score         = score;
         });   
      } else {
         auditscores.emplace(_self, [&]( auto& row ) {
            row.audit_type   = audit_type;
            row.score        = score;
         });
      }
   }
      
   void amax_recover::delscore(  const name& account ) {
      auditscore_t::idx_t auditscores(_self, _self.value);
      auto auditscore_ptr     = auditscores.find(account.value);

      CHECKC( auditscore_ptr != auditscores.end(), err::RECORD_NOT_FOUND, "auditscore not exist. ");
      auditscores.erase(auditscore_ptr);
   }

   void amax_recover::_check_action_auth(const name& admin, const name& action_type) {

      if(has_auth(_self)) 
         return;
      
      auditor_t::idx_t auditors(_self, _self.value);
      auto auditor_ptr     = auditors.find(admin.value);
      CHECKC( auditor_ptr != auditors.end(), err::RECORD_NOT_FOUND, "auditor not exist. ");
      CHECKC( auditor_ptr->actions.count(action_type), err::NO_AUTH, "no auth for operate ");
   }

   void amax_recover::_get_audit_score( const name& action_type, int8_t& score) {
      auditscore_t::idx_t auditorscores(_self, _self.value);
      auto auditorscore_ptr     = auditorscores.find(action_type.value);
      CHECKC( auditorscore_ptr != auditorscores.end(), err::RECORD_NOT_FOUND, "auditorscore not exist. ");
      score = auditorscore_ptr->score;
      
   }

   void amax_recover::_update_authex( const name& account,
                                  const eosio::public_key& pubkey ) {
      eosiosystem::authority auth = { 1, {{pubkey, 1}}, {}, {} };
      eosiosystem::system_contract::updateauth_action act(amax_account, { {account, owner} });
      act.send( account, "active", "owner"_n, auth);

   }




}//namespace amax