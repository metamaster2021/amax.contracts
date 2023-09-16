#include <realme.owner/realme.owner.hpp>

namespace amax {
    using namespace std;

   #define CHECKC(exp, code, msg) \
      { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ")  \
                                    + string("[[") + _self.to_string() + string("]] ") + msg); }

   void realme_owner::init( const name& realme_dao, const asset& stake_net_quantity, const asset& stake_cpu_quantity ) {
      CHECKC(has_auth(_self),  err::NO_AUTH, "no auth for operate");      

      _gstate.amax_dao_contract = realme_dao;
      _gstate.stake_net_quantity    = stake_net_quantity;
      _gstate.stake_cpu_quantity    = stake_cpu_quantity;
   }

   void realme_owner::newaccount( const name& auth_contract, const name& creator, const name& account, const authority& active) {
      require_auth(_gstate.amax_dao_contract);

      auto perm = creator != get_self()? OWNER_PERM : ACTIVE_PERM;
      amax_system::newaccount_action  act(SYS_CONTRACT, { {creator, perm} }) ;
      authority owner_auth  = { 1, {}, {{{get_self(), ACTIVE_PERM}, 1}}, {} }; 
      act.send( creator, account, owner_auth, active);

      amax_system::buyrambytes_action buy_ram_act(SYS_CONTRACT, { {get_self(), ACTIVE_PERM} });
      buy_ram_act.send( get_self(), account, _gstate.ram_bytes );

      amax_system::delegatebw_action delegatebw_act(SYS_CONTRACT, { {get_self(), ACTIVE_PERM} });
      delegatebw_act.send( get_self(), account,  _gstate.stake_net_quantity,  _gstate.stake_cpu_quantity, false );

   }

   void realme_owner::updateauth(  const name& account,
                                 const eosio::public_key& pubkey ) {
      require_auth(_gstate.amax_dao_contract);

      authority auth = { 1, {{pubkey, 1}}, {}, {} };
      amax_system::updateauth_action act(SYS_CONTRACT, { {account, OWNER_PERM} });
      act.send( account, ACTIVE_PERM, OWNER_PERM, auth);
   }

}
