#pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>


#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>


namespace amax {

using namespace std;
using namespace eosio;

namespace ActionPermType {
    static constexpr eosio::name NEWACCOUNT     {"newaccount"_n };
    static constexpr eosio::name BINDINFO       {"bindinfo"_n };
    static constexpr eosio::name CREATECORDER   {"createorder"_n };
    static constexpr eosio::name SETSCORE       {"setscore"_n };
}

#define TBL struct [[eosio::table, eosio::contract("amax.checker")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("amax.checker")]]

NTBL("global") global_t {
    name                     amax_recover_contract;
    name                     amax_proxy_contract;
    name                     checker_type;

    EOSLIB_SERIALIZE( global_t, (amax_recover_contract)(amax_proxy_contract)(checker_type))
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

//scope: _self 
TBL account_info_t {
    name                        account;    
    string                      audit_info;
    time_point_sec              created_at;

    account_info_t() {}
    account_info_t(const name& i): account(i) {}

    uint64_t primary_key()const { return account.value ; }

    typedef eosio::multi_index< "acctinfos"_n,  account_info_t> idx;

    EOSLIB_SERIALIZE( account_info_t, (account)(audit_info)(created_at) )
};

TBL checker_t {
    name                    checker;              //PK
    set<name>               actions;         

    checker_t() {}
    checker_t(const name& i): checker(i) {}

    uint64_t primary_key()const { return checker.value; }

    typedef eosio::multi_index< "checkers"_n,  checker_t > idx_t;

    EOSLIB_SERIALIZE( checker_t, (checker)(actions) )
};

} //namespace amax