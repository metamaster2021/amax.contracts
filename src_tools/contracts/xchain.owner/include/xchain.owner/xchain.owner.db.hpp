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
#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)


static constexpr eosio::symbol SYS_SYMB         = SYMBOL("AMAX", 8);
static constexpr eosio::name SYS_CONTRACT       = "amax"_n;
static constexpr eosio::name OWNER_PERM         = "owner"_n;
static constexpr eosio::name ACTIVE_PERM        = "active"_n;

#define TBL struct [[eosio::table, eosio::contract("realme.owner")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("realme.owner")]]

NTBL("global") global_t {
    name        admin                   = "armoniaadmin"_n;
    uint64_t    ram_bytes               = 5000;
    asset       stake_net_quantity      = asset(10000, SYS_SYMB); //TODO: set params in global table
    asset       stake_cpu_quantity      = asset(40000, SYS_SYMB);
    EOSLIB_SERIALIZE( global_t, (admin)(ram_bytes)(stake_net_quantity)(stake_cpu_quantity))
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

} //namespace amax