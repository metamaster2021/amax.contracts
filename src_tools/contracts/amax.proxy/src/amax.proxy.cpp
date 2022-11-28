#include <amax.proxy/amax.proxy.hpp>

namespace amax {
    using namespace std;

    #define CHECKC(exp, code, msg) \
        { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ") + msg); }


   void amax_proxy::init( const name& amax_recover ) {
      CHECKC(has_auth(_self),  err::NO_AUTH, "no auth for operate");      

      _gstate.amax_recover_contract =  amax_recover;
   }

    void amax_proxy::setauditor( const name& account, const set<name>& actions ) {
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

    void amax_proxy::delauditor(  const name& account ) {
      CHECKC(has_auth(_self),  err::NO_AUTH, "no auth for operate");      

      auditor_t::idx_t auditors(_self, _self.value);
      auto auditor_ptr     = auditors.find(account.value);

      CHECKC( auditor_ptr != auditors.end(), err::RECORD_EXISTING, "auditor not exist. ");
      auditors.erase(auditor_ptr);
   }

   void amax_proxy::_check_action_auth(const name& admin, const name& action_type) {
      if(has_auth(_self)) 
         return;
      auditor_t::idx_t auditors(_self, _self.value);
      auto auditor_ptr     = auditors.find(admin.value);
      CHECKC( auditor_ptr != auditors.end(), err::RECORD_NOT_FOUND, "auditor not exist. ");
      CHECKC( auditor_ptr->actions.count(action_type), err::NO_AUTH, "no auth for operate ");
      CHECKC(has_auth(admin),  err::NO_AUTH, "no auth for operate");      
   }

   //创建账号
   //   1. set auth

   //buy ram
   //buy cpu
   //amax.recover 创建bind 记录 

   void amax_proxy::newaccount( const name& checker_contract, const name& creator, const name& account, const authority& active) {
      require_auth(checker_contract);

      auto perm = creator != get_self()? OWNER_PERM : ACTIVE_PERM;
      amax_system::newaccount_action  act(SYS_CONTRACT, { {creator, perm} }) ;
      authority owner_auth  = { 1, {}, {{{get_self(), ACTIVE_PERM}, 1}}, {} }; 
      act.send( creator, account,  owner_auth, active);

      amax_system::buyrambytes_action buy_ram_act(SYS_CONTRACT, { {get_self(), ACTIVE_PERM} });
      buy_ram_act.send( get_self(), account, 10000 ); //TODO

      auto stake_net_quantity = asset(100000, SYS_SYMB); //TODO: set params in global table
      auto stake_cpu_quantity = asset(100000, SYS_SYMB);
      amax_system::delegatebw_action delegatebw_act(SYS_CONTRACT, { {get_self(), ACTIVE_PERM} });
      delegatebw_act.send( get_self(), account,stake_net_quantity, stake_cpu_quantity,  false );

      amax_recover::bindaccount_action bindaccount_act(_gstate.amax_recover_contract, { {get_self(), ACTIVE_PERM} });
      bindaccount_act.send( account, checker_contract);
   }

   void amax_proxy::updateauth(  const name& account,
                                 const eosio::public_key& pubkey ) {
      require_auth(_gstate.amax_recover_contract);

      authority auth = { 1, {{pubkey, 1}}, {}, {} };
      amax_system::updateauth_action act(SYS_CONTRACT, { {account, OWNER_PERM} });
      act.send( account, ACTIVE_PERM, OWNER_PERM, auth);
   }

}
