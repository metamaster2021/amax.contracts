#pragma once

#include "wasm_db.hpp"

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

using namespace eosio;
using namespace std;
using std::string;

// using namespace wasm;
#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

static constexpr eosio::name active_perm        {"active"_n};
static constexpr symbol SYS_SYMB                = SYMBOL("AMAX", 8);
static constexpr name SYS_BANK                  { "amax.token"_n };

namespace wasm { namespace db {

#define TBL [[eosio::table, eosio::contract("amax.bootdao")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("amax.bootdao")]]

NTBL("global") global_t {
    set<name> whitelist_accounts;

    EOSLIB_SERIALIZE( global_t, (whitelist_accounts) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;



} }