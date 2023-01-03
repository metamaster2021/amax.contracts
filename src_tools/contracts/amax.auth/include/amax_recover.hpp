#pragma once

#include <eosio/action.hpp>
#include <eosio/print.hpp>
#include <wasm_db.hpp>
#include <amax_system.hpp>

namespace amax {

   typedef std::variant<eosio::public_key, string> recover_target_type;

   using eosio::checksum256;
   using eosio::ignore;
   using eosio::name;
   using eosio::permission_level;
   using eosio::public_key;
   using amax::authority;

class amax_recover {
      public: 
      
            [[eosio::action]] 
            void newaccount( const name& auth_contract, const name& creator, const name& account, const authority& active );

            [[eosio::action]] 
            ACTION checkauth( const name& auth_contract, const name& account );

            [[eosio::action]] 
            ACTION setscore( const name& auth_contract, const name& account, const uint64_t& order_id, const uint8_t& score);

            [[eosio::action]] 
            ACTION createorder(
                     const uint64_t&            sn,
                     const name&                auth_contract,
                     const name&                account,
                     const bool&                manual_check_required,
                     const uint8_t&             score,
                     const recover_target_type& recover_target);
            

            using newaccount_action       = eosio::action_wrapper<"newaccount"_n,   &amax_recover::newaccount>;
            using checkauth_action        = eosio::action_wrapper<"checkauth"_n,    &amax_recover::checkauth>;
            using setscore_action         = eosio::action_wrapper<"setscore"_n,     &amax_recover::setscore>;
            using createcorder_action     = eosio::action_wrapper<"createorder"_n,  &amax_recover::createorder>;
};

}