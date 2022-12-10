#include <amax.auth/amax.auth.hpp>

static constexpr eosio::name ACTIVE_PERM        = "active"_n;

namespace amax {
    using namespace std;

   #define CHECKC(exp, code, msg) \
      { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ")  \
                                    + string("[[") + _self.to_string() + string("]] ") + msg); }

   void amax_auth::init( const name& amax_recover, const name& amax_proxy) {
      CHECKC(has_auth(_self),  err::NO_AUTH, "no auth for operate");

      _gstate.amax_recover_contract    = amax_recover;
      _gstate.amax_proxy_contract      = amax_proxy;
   }

   void amax_auth::newaccount(const name& auth, const name& creator, const name& account, const string& info, const authority& active) {
      _check_action_auth(auth, ActionType::NEWACCOUNT);

      //check account in amax.recover
      account_realme_t accountrealme(account);
      CHECKC( !_dbc.get(accountrealme) , err::RECORD_EXISTING, "account info already exist. ");
      accountrealme.realme_info  = info;
      accountrealme.created_at   = current_time_point();
      _dbc.set(accountrealme, _self);

      amax_proxy::newaccount_action newaccount_act(_gstate.amax_proxy_contract, { {get_self(), ACTIVE_PERM} });
      newaccount_act.send(get_self(), creator, account, active);
   }

   void amax_auth::bindinfo ( const name& auth, const name& account, const string& info) {
      _check_action_auth(auth, ActionType::BINDINFO);

      CHECKC(is_account(account), err::PARAM_ERROR,  "account invalid: " + account.to_string());
      //check account in amax.recover

      account_realme_t accountrealme(account);
      CHECKC( !_dbc.get(accountrealme) , err::RECORD_EXISTING, "account info already exist. ");
      accountrealme.realme_info  = info;
      accountrealme.created_at   = current_time_point();
      _dbc.set(accountrealme, _self);

      amax_recover::checkauth_action checkauth_act(_gstate.amax_recover_contract, { {get_self(), ACTIVE_PERM} });
      checkauth_act.send( get_self(),  account);
   }

   void amax_auth::createorder(  
                        const uint64_t&            sn,
                        const name&                auth,
                        const name&                account,
                        const bool&                manual_check_required,
                        const uint8_t&             score,
                        const recover_target_type& recover_target) {
      _check_action_auth(auth, ActionType::CREATECORDER);
      amax_recover::createcorder_action createcorder_act(_gstate.amax_recover_contract, { {get_self(), ACTIVE_PERM} });
      createcorder_act.send( sn, get_self(), account, manual_check_required, score, recover_target);
   }

   void amax_auth::setscore(const name& auth,
                                 const name& account,
                                 const uint64_t& order_id,
                                 const uint8_t& score ) {

      _check_action_auth(auth, ActionType::SETSCORE);
      amax_recover::setscore_action setscore_act(_gstate.amax_recover_contract, { {get_self(), ACTIVE_PERM} });
      setscore_act.send(get_self(), account, order_id, score);
   }

    void amax_auth::setauth( const name& auth, const set<name>& actions ) {
      require_auth(_self);      
      CHECKC(is_account(auth), err::PARAM_ERROR,  "account invalid: " + auth.to_string());

      auth_t::idx_t auths(_self, _self.value);
      auto auth_ptr = auths.find(auth.value);

      if( auth_ptr != auths.end() ) {
         auths.modify(*auth_ptr, _self, [&]( auto& row ) {
            row.actions      = actions;
         });   
      } else {
         auths.emplace(_self, [&]( auto& row ) {
            row.auth      = auth;
            row.actions      = actions;
         });
      }
   }

    void amax_auth::delauth(  const name& account ) {
      require_auth(_self);    

      auth_t::idx_t auths(_self, _self.value);
      auto auth_ptr     = auths.find(account.value);

      CHECKC( auth_ptr != auths.end(), err::RECORD_EXISTING, "auth not exist. ");
      auths.erase(auth_ptr);
   }

   void amax_auth::_check_action_auth(const name& auth, const name& action_type) {
      CHECKC(has_auth(auth),  err::NO_AUTH, "no auth for operate: " + auth.to_string());      

      auto auth_itr     = auth_t(auth);
      CHECKC( _dbc.get(auth_itr), err::RECORD_NOT_FOUND, "amax_auth auth not exist. ");
      CHECKC( auth_itr.actions.count(action_type), err::NO_AUTH, "amax_auth no action for " + auth.to_string());
   }

}
