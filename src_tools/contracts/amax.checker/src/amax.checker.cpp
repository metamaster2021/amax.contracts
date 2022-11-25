#include <amax.checker/amax.checker.hpp>
namespace amax {
    using namespace std;

    #define CHECKC(exp, code, msg) \
        { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ") + msg); }


   void amax_checker::init( const name& amax_recover ) {
      _gstate.amax_recover_contract =  amax_recover;
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

      amax_recover::addauth_action addauth_act(_gstate.amax_recover_contract, { {get_self(), "active"_n} });
      addauth_act.send( account);

   }

   void amax_checker::setscore(const name& admin,
                                 const name& account,
                                 const uint64_t& order_id,
                                 const uint8_t& score ) {

      _check_action_auth(admin, ActionPermType::SETSCORE);
      amax_recover::setscore_action setscore_act(_gstate.amax_recover_contract, { {get_self(), "active"_n} });
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
      CHECKC( auditor_ptr != auditors.end(), err::RECORD_NOT_FOUND, "auditor not exist. ");
      CHECKC( auditor_ptr->actions.count(action_type), err::NO_AUTH, "no auth for operate ");
      CHECKC(has_auth(admin),  err::NO_AUTH, "no auth for operate");      
   }

}
