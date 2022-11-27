#pragma once

#include <eosio/action.hpp>
#include <eosio/print.hpp>
#include <amax_proxy.hpp>


namespace amax {

   typedef std::variant<eosio::public_key, string> recover_target_type;

   using eosio::checksum256;
   using eosio::ignore;
   using eosio::name;
   using eosio::permission_level;
   using eosio::public_key;

class amax_recover {
      public: 
      
            [[eosio::action]] 
            void bindaccount( const name& account );

            [[eosio::action]] 
            ACTION checkauth( const name& checker_contract, const name& account );

            [[eosio::action]] 
            ACTION setscore( const name& checker_contract, const name& account, const uint64_t& order_id, const uint8_t& score);

            [[eosio::action]] 
            ACTION createcorder(
                     const name&                checker_contract,
                     const name&                account,
                     const recover_target_type& recover_target,
                     const bool&                manual_check_required,
                     const uint8_t&             score);
            

            using bindaccount_action      = eosio::action_wrapper<"bindaccount"_n,  &amax_recover::bindaccount>;
            using checkauth_action        = eosio::action_wrapper<"checkauth"_n,    &amax_recover::checkauth>;
            using setscore_action         = eosio::action_wrapper<"setscore"_n,     &amax_recover::setscore>;
            using createcorder_action     = eosio::action_wrapper<"createcorder"_n, &amax_recover::createcorder>;
};

}