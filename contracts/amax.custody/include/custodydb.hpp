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

static constexpr symbol SYS_SYMBOL              = SYMBOL("AMAX", 8);
static constexpr name SYS_BANK                  { "amax.token"_n };
static constexpr name COUNTER_ADMIN             = "admin"_n;
static constexpr name COUNTER_PLAN              = "plan"_n;
static constexpr name COUNTER_STAKE             = "stake"_n;

static const string COUNTER_IDX                 = "ID";

static constexpr uint16_t PERCENT_BOOST         = 10000;
static constexpr int128_t HIGH_PRECISION_1      = 100000000000000000;   //10^17
static constexpr int128_t PRECISION_1           = 100000000;            //10^8
static constexpr int128_t PRECISION             = 8;

static constexpr uint64_t MAX_LOCK_DAYS         = 365 * 10;
static constexpr uint64_t DAY_SECONDS           = 24 * 60 * 60;
// static constexpr int64_t DAY_SECONDS        =  60; //for testing only

namespace wasm { namespace db {

#define CUSTODY_TBL [[eosio::table, eosio::contract("custody")]]

struct [[eosio::table("global"), eosio::contract("custody")]] global_t {
    name admin;             // default is contract self
    bool initialized        = false; 

    EOSLIB_SERIALIZE( global_t, (admin)(initialized) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

/* (unlock_days ->  Pair(accmulated_unlock_ratio% * 10000, proposal_txid) */
typedef std::map<uint16_t, pair<uint16_t, checksum256>> unlock_plan_map;

struct CUSTODY_TBL plan_t {
    uint64_t        plan_id;

    string          plan_name;                  //E.g. WGRT_4yearlock | WGRT_5yearlock
    name            asset_contract;             //asset issuing contract (ARC20)
    symbol          asset_symbol;               //E.g. AMAX | CNYD
    uint64_t        unlock_interval_days;       //interval between two consecutive unlock timepoints
    uint64_t        unlock_times;

    uint64_t        total_staked_amount = 0;     //sats: updated upon stake
    uint64_t        total_redeemed_amount = 0;   //sats: updated upon redeem

    uint64_t primary_key() const { return plan_id; }
    uint64_t scope() const { return 0; }

    plan_t() {}
    plan_t(uint64_t pid): plan_id(pid) {}
    plan_t(uint64_t pid, string pn, name ac, symbol as, uint64_t uid, uint64_t ut):
        plan_id(pid), plan_name(pn), asset_contract(ac), asset_symbol(as), unlock_interval_days(uid), unlock_times(ut) {}

    typedef eosio::multi_index<"plans"_n, plan_t> tbl_t;

    EOSLIB_SERIALIZE( plan_t, (plan_id)(plan_name)(asset_contract)(asset_symbol)(unlock_interval_days)(unlock_times)
                              (total_staked_amount)(total_redeemed_amount) )

};
struct CUSTODY_TBL stake_t {
    name                recipient;         // scope
    uint64_t            stake_id;          // PK, unique within the contract

    uint64_t            plan_id;
    uint64_t            staked_amount;
    uint64_t            redeemed_amount = 0;//updated upon redeem
    time_point          created_at;         //stake time (UTC time)
    time_point          updated_at;         //update time

    uint64_t primary_key() const { return stake_id; }
    uint64_t       scope() const { return recipient.value; }

    stake_t() {}
    stake_t(name r, uint64_t s): recipient(r), stake_id(s) {}
    stake_t(uint64_t p, name r, uint64_t s, uint64_t sa, time_point c): 
             plan_id(p), recipient(r), stake_id(s), staked_amount(sa), created_at(c) {}

    typedef eosio::multi_index<"stakes"_n, stake_t> tbl_t;

    EOSLIB_SERIALIZE( stake_t,  (recipient)(stake_id)(plan_id)(staked_amount)
                                (redeemed_amount)(created_at)(updated_at) )
};


} }