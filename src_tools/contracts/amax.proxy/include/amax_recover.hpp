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
            void bindaccount( const name& account, const name& default_auth );

            using bindaccount_action = eosio::action_wrapper<"bindaccount"_n, &amax_recover::bindaccount>;

};

}