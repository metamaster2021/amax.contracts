#include <amax.checker/amax.checker.hpp>

static constexpr eosio::name ACTIVE_PERM        = "active"_n;

namespace amax {
    using namespace std;

    #define CHECKC(exp, code, msg) \
        { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ") + msg); }


   void amax_checker::init( const name& amax_recover, const name& amax_proxy) {
      _gstate.amax_recover_contract    = amax_recover;
      _gstate.amax_proxy_contract      = amax_proxy;
   }

   void amax_checker::newaccount(const name& admin, const name& creator, const name& account, const authority& active, const string& info) {
      _check_action_auth(admin, ActionPermType::NEWACCOUNT);

      //check account in amax.recover

      accountinfo_t::idx accountinfos(_self, _self.value);
      auto info_ptr     = accountinfos.find(account.value);
      CHECKC( info_ptr == accountinfos.end(), err::RECORD_EXISTING, "account info already exist. ");
      auto now           = current_time_point();

      accountinfos.emplace( _self, [&]( auto& row ) {
         row.account 		   = account;
         row.info             = info;
         row.created_at       = now;
      });

      amax_proxy::newaccount_action newaccount_act(_gstate.amax_proxy_contract, { {get_self(), ACTIVE_PERM} });
      newaccount_act.send(get_self(), creator, account, active);
   }

   void amax_checker::bindinfo ( const name& admin, const name& account, const string& info) {

      _check_action_auth(admin, ActionPermType::BINDINFO);

      check(is_account(account), "account invalid: " + account.to_string());
      //check account in amax.recover

      accountinfo_t::idx accountinfos(_self, _self.value);
      auto info_ptr     = accountinfos.find(account.value);
      CHECKC( info_ptr == accountinfos.end(), err::RECORD_EXISTING, "account info already exist. ");
      auto now           = current_time_point();

      accountinfos.emplace( _self, [&]( auto& row ) {
         row.account 		   = account;
         row.info             = info;
         row.created_at       = now;
      });

      amax_recover::checkauth_action checkauth_act(_gstate.amax_recover_contract, { {get_self(), ACTIVE_PERM} });
      checkauth_act.send( account);
   }


   void amax_checker::createcorder(  const name& admin,
                        const name& account,
                        const recover_target_type& recover_target,
                        const bool& manual_check_required,
                        const uint8_t& score) {
      _check_action_auth(admin, ActionPermType::CREATECORDER);
      amax_recover::createcorder_action createcorder_act(_gstate.amax_recover_contract, { {get_self(), ACTIVE_PERM} });
      createcorder_act.send( account, recover_target, manual_check_required, score);
   }

   void amax_checker::setscore(const name& admin,
                                 const name& account,
                                 const uint64_t& order_id,
                                 const uint8_t& score ) {

      _check_action_auth(admin, ActionPermType::SETSCORE);
      amax_recover::setscore_action setscore_act(_gstate.amax_recover_contract, { {get_self(), ACTIVE_PERM} });
      setscore_act.send( account, order_id, score);
   }

    void amax_checker::setauditor( const name& account, const set<name>& actions ) {
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

    void amax_checker::delauditor(  const name& account ) {
      CHECKC(has_auth(_self),  err::NO_AUTH, "no auth for operate");      

      auditor_t::idx_t auditors(_self, _self.value);
      auto auditor_ptr     = auditors.find(account.value);

      CHECKC( auditor_ptr != auditors.end(), err::RECORD_EXISTING, "auditor not exist. ");
      auditors.erase(auditor_ptr);
   }

   void amax_checker::_check_action_auth(const name& admin, const name& action_type) {
      if(has_auth(_self)) 
         return;
      auditor_t::idx_t auditors(_self, _self.value);
      auto auditor_ptr     = auditors.find(admin.value);
      CHECKC( auditor_ptr != auditors.end(), err::RECORD_NOT_FOUND, "amax_checker auditor not exist. ");
      CHECKC( auditor_ptr->actions.count(action_type), err::NO_AUTH, "amax_checker no action for " + admin.to_string());
      CHECKC(has_auth(admin),  err::NO_AUTH, "no auth for operate" + admin.to_string());      
   }

}
