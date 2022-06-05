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

static constexpr uint64_t MAX_LOCK_DAYS         = 365 * 10;

#ifndef DAY_SECONDS_FOR_TEST
static constexpr uint64_t DAY_SECONDS           = 24 * 60 * 60;
#else
#warning "DAY_SECONDS_FOR_TEST should be used only for test!!!"
static constexpr uint64_t DAY_SECONDS           = DAY_SECONDS_FOR_TEST;
#endif//DAY_SECONDS_FOR_TEST

static constexpr uint32_t MAX_TITLE_SIZE        = 64;


namespace wasm { namespace db {

#define bootdao_TBL [[eosio::table, eosio::contract("amax.bootdao")]]
#define bootdao_TBL_NAME(name) [[eosio::table(name), eosio::contract("amax.bootdao")]]

struct bootdao_TBL_NAME("global") global_t {
    set<name> whitelist_accounts;

    EOSLIB_SERIALIZE( global_t, (whitelist_accounts) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;



} }