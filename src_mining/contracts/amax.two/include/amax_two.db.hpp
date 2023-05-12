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
#define hash(str) sha256(const_cast<char*>(str.c_str()), str.size())
static constexpr symbol SYS_SYMBOL              = SYMBOL("AMAX", 8);

static constexpr name SYS_BANK                  { "amax.token"_n };
static constexpr name APL_CONTRACT              { "aplink.token"_n };
static constexpr name PHASES                    { "second"_n };

static constexpr uint64_t AMAX_PRECISION        = 1'0000'0000;

static constexpr uint64_t MAX_LOCK_DAYS         = 365 * 10;

#ifndef HOUR_SECONDS_FOR_TEST
static constexpr uint64_t HOUR_SECONDS           =  60 * 60;
#else
#warning "DAY_SECONDS_FOR_TEST should be used only for test!!!"
static constexpr uint64_t HOUR_SECONDS           = HOUR_SECONDS_FOR_TEST;
#endif//HOUR_SECONDS_FOR_TEST

static constexpr uint32_t MAX_TITLE_SIZE        = 64;


namespace wasm { namespace db {

#define CUSTODY_TBL [[eosio::table, eosio::contract("amax.two")]]
#define CUSTODY_TBL_NAME(name) [[eosio::table(name), eosio::contract("amax.two")]]

struct CUSTODY_TBL_NAME("global") global_t {
    name                mine_token_contract;
    name                admin;
    time_point_sec      started_at;
    time_point_sec      ended_at;
    asset               mine_token_total = asset(0,SYS_SYMBOL);
    asset               mine_token_remained = asset(0,SYS_SYMBOL);
    
    EOSLIB_SERIALIZE( global_t, (mine_token_contract)(admin)(started_at)(ended_at)(mine_token_total)(mine_token_remained) )
};

typedef eosio::singleton< "global"_n, global_t > global_singleton;

} }