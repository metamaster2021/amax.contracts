#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

#include <string>

namespace eosiosystem {
   class system_contract;
}

namespace amax {

   using std::string;

   class [[eosio::contract("amax.token")]] token : public contract {
      public:
         using contract::contract;

         static bool is_blacklisted( const name& token_contract, const name& target ) {
            blackaccounts black_accts( token_contract, token_contract.value );
            return ( black_accts.find( target.value ) != black_accts.end() );
         }


      private:
         struct [[eosio::table]] blacklist_t {
            name     account;

            uint64_t primary_key()const { return account.value; }
         };

         typedef eosio::multi_index< "blacklist"_n, blacklist_t > blackaccounts;

   };

}
