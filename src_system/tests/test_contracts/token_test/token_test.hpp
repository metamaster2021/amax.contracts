#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

using namespace eosio;
using namespace std;

class [[eosio::contract]] token_test : public eosio::contract
{
public:
    using eosio::contract::contract;

    [[eosio::action]] void testtransfer(const name &from,
                                        const name &to,
                                        const asset &quantity,
                                        const string &memo);

    [[eosio::on_notify("amax.token::transfer")]] void ontransfer(name from, name to, asset quantity, string memo);

    struct [[eosio::table]] account
    {
        name acct;

        uint64_t primary_key() const { return acct.value; }
    };

   typedef eosio::multi_index< "accounts"_n, account > accounts;
};
