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

    EOSLIB_SERIALIZE( global_t, (amax_recover_contract)(amax_proxy_contract))
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

TBL account_info_t {
    name                        account;    
    string                      info;
    time_point_sec              created_at;

    account_info_t() {}
    account_info_t(const name& i): account(i) {}

    uint64_t primary_key()const { return account.value ; }

    typedef eosio::multi_index< "acctinfos"_n,  account_info_t> idx;

    EOSLIB_SERIALIZE( account_info_t, (account)(info)(created_at) )
};

TBL auditor_t {
    name                    account;              //PK
    set<name>               actions;         

    auditor_t() {}
    auditor_t(const name& i): account(i) {}

    uint64_t primary_key()const { return account.value; }

    typedef eosio::multi_index< "auditors"_n,  auditor_t > idx_t;

    EOSLIB_SERIALIZE( auditor_t, (account)(actions) )
};

} //namespace amax