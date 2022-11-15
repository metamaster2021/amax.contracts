#include <amax.recover/amax.recover.hpp>
// #include <amax.system/amax.system.hpp>

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

   void amax_recover::init( const name& admin, const uint8_t& score_limit) {
      require_auth( _self );
      CHECKC( is_account( admin ), err::PARAM_ERROR, "admin account does not exist");
      _gstate.admin              = admin;
      _gstate.score_limit        = score_limit;
   }

   void amax_recover::bindaccount (const name& account, const checksum256& mobile_hash ) {
      CHECKC( has_auth(_self) || has_auth(_gstate.admin), err::NO_AUTH, "no auth for operate" )
      accountaudit_t::idx accountaudits(_self, _self.value);
      auto audit_ptr     = accountaudits.find(account.value);
      CHECKC( audit_ptr == accountaudits.end(), err::RECORD_EXISTING, "order already exist. ");
      auto now                   = current_time_point();

      accountaudits.emplace( _self, [&]( auto& row ) {
         row.account 		   = account;
         row.mobile_hash      = mobile_hash;
         row.created_at          = now;
      });   
   }

   void amax_recover::bindanswer(const name& account, map<uint8_t, checksum256 >& answers ) {
      
      CHECKC( has_auth(_self) || has_auth(_gstate.admin), err::NO_AUTH, "no auth for operate" )
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
                        const checksum256& new_pubkey,
                        const bool& manual_check_flag) {
      _check_action_auth(admin, ActionPermType::CREATEORDER);

      accountaudit_t::idx accountaudits(_self, _self.value);
      auto audit_ptr     = accountaudits.find(account.value);
      CHECKC( audit_ptr != accountaudits.end(), err::RECORD_NOT_FOUND, "account not exist. ");
      CHECKC( mobile_hash == audit_ptr->mobile_hash, err::PARAM_ERROR, "mobile hash check failed" )


      updateorder_t::idx_t orders( _self, _self.value );
      auto xinto_index 			         = orders.get_index<"accountidx"_n>();
      auto order_itr 			         = orders.find( account.value );
      CHECKC( order_itr == orders.end(), err::RECORD_EXISTING, "order already existed. ");

      name manual_check_status = ManualCheckStatus::NONEED;
      auto duration_second    = order_expired;
      if (manual_check_flag) {
         manual_check_status   = ManualCheckStatus::NEED;
         duration_second      = manual_order_expired;
      }

      _gstate.last_order_id ++;
      auto order_id           = _gstate.last_order_id; 
      auto now                = current_time_point();
      int8_t mobile_check_score = -1;
      _get_audit_score(AuditType::MOBILENO, mobile_check_score);

      orders.emplace( _self, [&]( auto& row ) {
         row.id 					      = order_id;
         row.account 			      = account;
         row.update_action_type     = UpdateActionType::PUBKEY;
         row.new_pubkey             = new_pubkey;
         row.mobile_check_score     = mobile_check_score;       
         row.question_check_score   = -1;         
         row.did_check_score        = -1;
         row.manual_check_status    = manual_check_status;
         row.pay_status             = PayStatus::NOPAY;
         row.created_at             = now;
         row.expired_at             = current_time_point() + eosio::seconds(duration_second);;
         row.updated_at             = now;
      });
   
   }

   void amax_recover::chkanswer( const name& admin, const uint64_t& order_id, const name& account, const int8_t& score) {
      _check_action_auth(admin, ActionPermType::CHKANSWER);
      updateorder_t::idx_t orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not found. ");
      int8_t answer_score_limit = 0;
      _get_audit_score(AuditType::ANSWER, answer_score_limit);
      CHECKC(answer_score_limit >= score, err::PARAM_ERROR, "scores exceed limit")

      orders.modify(*order_ptr, _self, [&]( auto& row ) {
         row.did_check_score     = score;
         row.updated_at          = current_time_point();
      });
   }

   void amax_recover::chkdid( const name& admin, const uint64_t& order_id, const name& account, const bool& passed) {
      _check_action_auth(admin, ActionPermType::CHKDID);
      
      updateorder_t::idx_t orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not found. ");
      
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

      updateorder_t::idx_t orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not found. ");

      name manual_check_status       = ManualCheckStatus::FAILED;
      if (passed) manual_check_status   = ManualCheckStatus::PASSED;
      auto now                = current_time_point();
      orders.modify(*order_ptr, _self, [&]( auto& row ) {
         row.manual_check_status    = manual_check_status;
         row.updated_at             = now;
      });
   }

   void amax_recover::closeorder( const name& submitter, const uint64_t& order_id) {
      CHECKC( has_auth(submitter) , err::NO_AUTH, "no auth for operate" )
      updateorder_t::idx_t orders(_self, _self.value);
      auto order_ptr     = orders.find(order_id);
      CHECKC( order_ptr != orders.end(), err::RECORD_NOT_FOUND, "order not found. "); 
      auto total_score = 0;
      if(order_ptr->mobile_check_score > 0 ) total_score += order_ptr->mobile_check_score;
      if(order_ptr->question_check_score > 0 ) total_score += order_ptr->question_check_score;
      if(order_ptr->did_check_score > 0 ) total_score += order_ptr->did_check_score;
      CHECKC( total_score < _gstate.score_limit, err::SCORE_NOT_ENOUGH, "score not enough" );
      CHECKC( order_ptr->manual_check_status == ManualCheckStatus::NEED || order_ptr->manual_check_status == ManualCheckStatus::FAILED ,
               err::NEED_MANUAL_CHECK, "need manual check" );
      
   }

   void amax_recover::addauditor( const name& account, const set<name>& actions ) {
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

   void amax_recover::addscore( const name& audit_type, const int8_t& score ) {
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
      CHECKC( has_auth(admin), err::NO_AUTH, "no auth for operate" )

      auditor_t::idx_t auditors(_self, _self.value);
      auto auditor_ptr     = auditors.find(admin.value);
      CHECKC( auditor_ptr != auditors.end(), err::RECORD_NOT_FOUND, "auditor not exist. ");
      CHECKC( !auditor_ptr->actions.count(action_type), err::NO_AUTH, "no auth for operate ");
   }

   void amax_recover::_get_audit_score( const name& action_type, int8_t& score) {
      auditscore_t::idx_t auditorscores(_self, _self.value);
      auto auditorscore_ptr     = auditorscores.find(action_type.value);
      CHECKC( auditorscore_ptr != auditorscores.end(), err::RECORD_NOT_FOUND, "auditorscore not exist. ");
      score = auditorscore_ptr->score;
      
   }

   // void amax_recover::_update_authex( const name& account,
   //                                const authority& auth ) {
   //    eosiosystem::system_contract::updateauth_action act(amax_account, { {account, owner} });
   //    act.send( account, "active", "owner"_n, auth);

   // }


}//namespace amax