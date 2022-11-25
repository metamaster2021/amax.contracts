#pragma once

#include <eosio/action.hpp>
#include <eosio/print.hpp>


namespace amax {

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
            ACTION addauth( const name& account, const name& contract );

            [[eosio::action]] 
            ACTION setscore( const uint64_t& order_id, const uint8_t& score);

            using bindaccount_action      = eosio::action_wrapper<"bindaccount"_n,  &amax_recover::bindaccount>;
            using addauth_action          = eosio::action_wrapper<"addauth"_n,      &amax_recover::addauth>;
            using setscore_action         = eosio::action_wrapper<"setscore"_n,     &amax_recover::setscore>;
};

}