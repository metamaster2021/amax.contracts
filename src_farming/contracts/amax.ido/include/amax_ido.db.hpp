#pragma once

#include "wasm_db.hpp"

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include "utils.hpp"

using namespace eosio;
using namespace std;
using std::string;

// using namespace wasm;
#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

static constexpr eosio::name active_perm        {"active"_n};
static constexpr symbol SYS_SYMBOL              = SYMBOL("AMAX", 8);
static constexpr symbol USDT_SYMBOL             = SYMBOL("MUSDT", 6);
static constexpr name SYS_BANK                  { "amax.token"_n };
static constexpr name USDT_BANK                 { "amax.mtoken"_n };

namespace wasm { namespace db {

#define ido_TBL [[eosio::table, eosio::contract("amax.ido")]]
#define IDO_TBL_NAME(name) [[eosio::table(name), eosio::contract("amax.ido")]]

struct IDO_TBL_NAME("global") global_t {
    asset   amax_price          = asset_from_string("100.000000 MUSDT");
    name    admin               = "armoniaadmin"_n;

    EOSLIB_SERIALIZE( global_t, (amax_price)(admin) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;


} }