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

#define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size())

static constexpr uint64_t seconds_per_day                   = 24 * 3600;
static constexpr uint64_t order_expiry_duration             = seconds_per_day;
static constexpr uint64_t manual_order_expiry_duration      = 3 * seconds_per_day;

static constexpr eosio::name RECOVER_ACCOUNT    = "amax.recover"_n;

static constexpr eosio::name AMAX_ACCOUNT       = "amax"_n;
static constexpr eosio::name OWNER_PERM         = "owner"_n;
static constexpr eosio::name ACTIVE_PERM        = "active"_n;

namespace ActionPermType {
    static constexpr eosio::name BINDINFO {"bindinfo"_n };
    static constexpr eosio::name SETSCORE {"setscore"_n };
}

#define TBL struct [[eosio::table, eosio::contract("amax.proxy")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("amax.proxy")]]

NTBL("global") global_t {
    name                     amax_recover_contract = RECOVER_ACCOUNT;

    EOSLIB_SERIALIZE( global_t, (amax_recover_contract))
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;


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