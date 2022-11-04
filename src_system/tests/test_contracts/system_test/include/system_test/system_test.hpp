#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/privileged.hpp>

#include <string>

namespace amax_system
{

    using std::string;
    using namespace eosio;
    using eosiosystem::authority;

    #define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

    static constexpr name active_permission       = "active"_n;

    class token {
        public:
            /**
             * Allows `from` account to transfer to `to` account the `quantity` tokens.
             * One account is debited and the other is credited with quantity tokens.
             *
             * @param from - the account to transfer from,
             * @param to - the account to be transferred to,
             * @param quantity - the quantity of tokens to be transferred,
             * @param memo - the memo string to accompany the transaction.
             */
            [[eosio::action]]
            void transfer( const name&    from,
                            const name&    to,
                            const asset&   quantity,
                            const string&  memo );


            using transfer_action = eosio::action_wrapper<"transfer"_n, &token::transfer>;
    };

    /**
     * system_test example for amax.system
     */
    class [[eosio::contract("system_test")]] system_test : public contract
    {
    public:
        using contract::contract;

         /**
          * Update authorization action updates pemission for an account.
          *
          * @param account - the account for which the permission is updated
          * @param pemission - the permission name which is updated
          * @param parem - the parent of the permission which is updated
          * @param auth - the json describing the permission authorization
          */
         [[eosio::action]]
         void updateauthex( const name& actor,
                            const name& required_permission,
                            const name& account,
                            const name& permission,
                            const name& parent,
                            const authority& auth );


         [[eosio::action]]
         void transferex(   const name& actor,
                            const name& required_permission,
                            const name& from,
                            const name& to,
                            const asset& quantity,
                            const std::string& memo );
    };

}
