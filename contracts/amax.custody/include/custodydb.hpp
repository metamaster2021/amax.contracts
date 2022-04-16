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
static constexpr symbol SYS_SYMBOL              = SYMBOL("AMAX", 8);
static constexpr name SYS_BANK                  { "amax.token"_n };

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
    bool initialized        = false;
    uint64_t trx_max_step   = 30;

    EOSLIB_SERIALIZE( global_t, (initialized)(trx_max_step) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

/* (unlock_days ->  Pair(accmulated_unlock_ratio% * 10000, proposal_txid) */
typedef std::map<uint16_t, pair<uint16_t, checksum256>> unlock_plan_map;

struct CUSTODY_TBL plan_t {
    uint64_t        id;
    name            owner;                      //plan owner
    string          title;                      //plan title: <=64 chars
    name            asset_contract;             //asset issuing contract (ARC20)
    symbol          asset_symbol;               //E.g. AMAX | CNYD
    uint64_t        unlock_interval_days;       //interval between two consecutive unlock timepoints
    uint64_t        unlock_times;
    uint64_t        total_issued = 0;    //stats: updated upon issue
    uint64_t        total_unlocked = 0;  //stats: updated upon unlock
    uint64_t        total_recycled = 0;
    bool            enabled = true;             //can be disabled
    time_point      created_at;                 //creation time (UTC time)
    time_point      updated_at;                 //update time: last updated at

    uint64_t primary_key() const { return id; }
    uint64_t scope() const { return 0; }

    uint64_t by_owner()const { return owner.value; }

    plan_t() {}
    plan_t(uint64_t pid): id(pid) {}
    plan_t(uint64_t pid, name o, string t, name ac, symbol as, uint64_t uid, uint64_t ut): id(pid), title(t), asset_contract(ac), asset_symbol(as), unlock_interval_days(uid), unlock_times(ut) {
        created_at = current_time_point();
    }

    typedef eosio::multi_index<"plans"_n, plan_t,
        indexed_by<"owneridx"_n,  const_mem_fun<plan_t, uint64_t, &plan_t::by_owner> >
    > tbl_t;

    EOSLIB_SERIALIZE( plan_t, (id)(owner)(title)(asset_contract)(asset_symbol)(unlock_interval_days)(unlock_times)
                              (total_issued)(total_unlocked)(enabled)(created_at)(updated_at) )

};
struct CUSTODY_TBL issue_t {
    uint64_t            plan_id;            // scope
    uint64_t            issue_id;           // PK, unique within the contract
    name                owner;
    uint64_t            issued;             //originally issued amount
    uint64_t            locked;             //currently locked amount
    uint64_t            unlocked;           //currently unlocked amount
    uint64_t            first_unlock_days;  //unlock since issued_at
    time_point          issued_at;          //issue time (UTC time)
    time_point          updated_at;         //update time: last unlocked at

    uint64_t       scope() const { return plan_id; }
    uint64_t primary_key() const { return issue_id; }

    issue_t() {}
    issue_t(uint64_t p, uint64_t s): plan_id(p), issue_id(s) {}
    issue_t(uint64_t p, uint64_t ii, name o, uint64_t is, uint64_t fu): plan_id(p), issue_id(ii), owner(o), issued(is), first_unlock_days(fu) {
        locked = issued;
        unlocked = 0;
        issued_at = current_time_point();
    }

    uint64_t by_owner() const { return owner.value; }
    uint128_t by_owner_update() const { return uint128_t(owner.value) << 64 | uint128_t(updated_at.sec_since_epoch()); }

    typedef eosio::multi_index<"issues"_n, issue_t,
        indexed_by<"ownerissues"_n,     const_mem_fun<issue_t, uint64_t, &issue_t::by_owner>>,
        indexed_by<"ownerupdate"_n,     const_mem_fun<issue_t, uint128_t, &issue_t::by_owner_update>>
    > tbl_t;

    EOSLIB_SERIALIZE( issue_t,  (plan_id)(issue_id)(owner)(issued)(locked)(unlocked)(first_unlock_days)(issued_at)(updated_at) )
};


} }