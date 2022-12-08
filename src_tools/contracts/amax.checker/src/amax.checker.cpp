#include <amax.checker/amax.checker.hpp>

static constexpr eosio::name ACTIVE_PERM        = "active"_n;

namespace amax {
    using namespace std;

    #define CHECKC(exp, code, msg) \
        { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ") + msg); }

   void amax_checker::init( const name& amax_recover, const name& amax_proxy) {
      CHECKC(has_auth(_self),  err::NO_AUTH, "no auth for operate");

      _gstate.amax_recover_contract    = amax_recover;
      _gstate.amax_proxy_contract      = amax_proxy;
   }

   void amax_checker::newaccount(const name& checker, const name& creator, const name& account, const string& info, const authority& active) {
      _check_action_auth(checker, ActionType::NEWACCOUNT);

      //check account in amax.recover
      account_realme_t accountrealme(account);
      CHECKC( !_dbc.get(accountrealme) , err::RECORD_EXISTING, "account info already exist. ");
      accountrealme.realme_info  = info;
      accountrealme.created_at   = current_time_point();
      _dbc.set(accountrealme);

      amax_proxy::newaccount_action newaccount_act(_gstate.amax_proxy_contract, { {get_self(), ACTIVE_PERM} });
      newaccount_act.send(get_self(), creator, account, active);
   }

   void amax_checker::bindinfo ( const name& checker, const name& account, const string& info) {
      _check_action_auth(checker, ActionType::BINDINFO);

      check(is_account(account), "account invalid: " + account.to_string());
      //check account in amax.recover

      account_realme_t accountrealme(account);
      CHECKC( !_dbc.get(accountrealme) , err::RECORD_EXISTING, "account info already exist. ");
      accountrealme.realme_info  = info;
      accountrealme.created_at   = current_time_point();
      _dbc.set(accountrealme);

      amax_recover::checkauth_action checkauth_act(_gstate.amax_recover_contract, { {get_self(), ACTIVE_PERM} });
      checkauth_act.send( get_self(),  account);
   }

   void amax_checker::createorder(  
                        const uint64_t&            sn,
                        const name&                checker,
                        const name&                account,
                        const bool&                manual_check_required,
                        const uint8_t&             score,
                        const recover_target_type& recover_target) {
      _check_action_auth(checker, ActionType::CREATECORDER);
      amax_recover::createcorder_action createcorder_act(_gstate.amax_recover_contract, { {get_self(), ACTIVE_PERM} });
      createcorder_act.send( sn, get_self(), account, manual_check_required, score, recover_target);
   }

   void amax_checker::setscore(const name& checker,
                                 const name& account,
                                 const uint64_t& order_id,
                                 const uint8_t& score ) {

      _check_action_auth(checker, ActionType::SETSCORE);
      amax_recover::setscore_action setscore_act(_gstate.amax_recover_contract, { {get_self(), ACTIVE_PERM} });
      setscore_act.send(get_self(), account, order_id, score);
   }

    void amax_checker::setchecker( const name& checker, const set<name>& actions ) {
      require_auth(_self);      

      checker_t::idx_t checkers(_self, _self.value);
      auto checker_ptr = checkers.find(checker.value);

      if( checker_ptr != checkers.end() ) {
         checkers.modify(*checker_ptr, _self, [&]( auto& row ) {
            row.actions      = actions;
         });   
      } else {
         checkers.emplace(_self, [&]( auto& row ) {
            row.checker      = checker;
            row.actions      = actions;
         });
      }
   }

    void amax_checker::delchecker(  const name& account ) {
      require_auth(_self);    

      checker_t::idx_t checkers(_self, _self.value);
      auto checker_ptr     = checkers.find(account.value);

      CHECKC( checker_ptr != checkers.end(), err::RECORD_EXISTING, "checker not exist. ");
      checkers.erase(checker_ptr);
   }

   void amax_checker::_check_action_auth(const name& checker, const name& action_type) {
      CHECKC(has_auth(checker),  err::NO_AUTH, "no auth for operate" + checker.to_string());      

      auto checker_itr     = checker_t(checker);
      CHECKC( _dbc.get(checker_itr), err::RECORD_NOT_FOUND, "amax_checker checker not exist. ");
      CHECKC( checker_itr.actions.count(action_type), err::NO_AUTH, "amax_checker no action for " + checker.to_string());
   }

}
