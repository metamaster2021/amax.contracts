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

static constexpr uint64_t MIN_APPROVE_COUNT     = 3;
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

struct CUSTODY_TBL admin_t {
    name admin;
    bool is_super_admin;

    uint64_t primary_key() const { return admin.value; }
    uint64_t scope() const { return 0; }

    admin_t() {}
    admin_t(name adminIn): admin(adminIn) {}
    admin_t(name adminIn, bool isSuperAdmin): admin(adminIn), is_super_admin(isSuperAdmin) {}

    typedef eosio::multi_index<"admins"_n, admin_t> tbl_t;

    EOSLIB_SERIALIZE( admin_t, (admin)(is_super_admin) )
};

struct CUSTODY_TBL plan_t {
    uint64_t        plan_id;

    string          plan_name;                  //E.g. WGRT_4yearlock | WGRT_5yearlock
    name            asset_bank;                 //asset issuing bank contract - E.g. wasm_bank, can also be a WRC20 bank contract
    symbol          asset_symbol;               //E.g. WGRT | WICC | XT
    uint16_t        unlock_days;                //100% unlocked afterwards, E.g. 4 * 365

    uint64_t        total_staked_amount = 0;        //updated upon stake
    uint64_t        total_redeemed_amount = 0;      //updated upon redeem

    unlock_plan_map  advance_unlock_plans;       //updated thru advance-unlock proposals

    uint64_t primary_key() const { return plan_id; }
    uint64_t scope() const { return 0; }

    plan_t() {}
    plan_t(uint16_t planId): plan_id(planId) {}
    plan_t(uint16_t planId, string planName, name bank, symbol symbolIn, uint16_t unlockDays):
        plan_id(planId), plan_name(planName), asset_bank(bank), asset_symbol(symbolIn), unlock_days(unlockDays) {}

    typedef eosio::multi_index<"plans"_n, plan_t> tbl_t;

    EOSLIB_SERIALIZE( plan_t, (plan_id)(plan_name)(asset_bank)(asset_symbol)(unlock_days)
                              (total_staked_amount)(total_redeemed_amount)(advance_unlock_plans) )

};

struct CUSTODY_TBL proposal_t {
    uint64_t            id;  //PK

    uint64_t            plan_id;
    uint64_t            advance_unlock_days;
    uint64_t            advance_unlock_ratio;
    uint64_t            approve_expired_at;  //the expired time of proposal

    std::set<name>      approval_admins;    //updated in approve process

    uint64_t primary_key() const { return id; }
    uint64_t scope() const { return 0; }

    proposal_t() {}
    proposal_t(uint64_t &pid): id(pid) {}
    proposal_t(const uint64_t& pid, uint64_t planId, uint64_t advanceUnlockDays, uint64_t advanceUnlockRatio,
                uint64_t approveExpiredAt): id(pid), plan_id(planId), advance_unlock_days(advanceUnlockDays),
                advance_unlock_ratio(advanceUnlockRatio), approve_expired_at(approveExpiredAt) {}

    typedef eosio::multi_index<"proposals"_n, proposal_t> tbl_t;

    EOSLIB_SERIALIZE( proposal_t, (id)(plan_id)(advance_unlock_days)(advance_unlock_ratio)(approve_expired_at)(approval_admins) )
};

struct CUSTODY_TBL counter_t {
    name            counter_key;    //stake_counter | admin_counter
    uint64_t        counter_val;

    uint64_t primary_key() const { return counter_key.value; }
    uint64_t scope() const { return 0; }

    counter_t() {}
    counter_t(name counterKey): counter_key(counterKey) {}

    typedef eosio::multi_index<"counters"_n, counter_t> tbl_t;

    EOSLIB_SERIALIZE( counter_t, (counter_key)(counter_val) )
};

struct CUSTODY_TBL stake_t {
    name                recipient;         // scope
    uint64_t            stake_id;          // PK, unique within the contract

    uint16_t            plan_id;
    uint64_t            staked_amount;
    uint64_t            redeemed_amount = 0;//updated upon redeem
    time_point          created_at;         //stake time (UTC time)
    time_point          updated_at;         //update time

    uint64_t primary_key() const { return stake_id; }
    uint64_t       scope() const { return recipient.value; }

    stake_t() {}
    stake_t(name r, uint64_t s): recipient(r), stake_id(s) {}
    stake_t(name r, uint64_t s, uint16_t p, uint64_t sa, time_point c): 
            recipient(r), stake_id(s), plan_id(p), staked_amount(sa), created_at(c) {}

    typedef eosio::multi_index<"stakes"_n, stake_t> tbl_t;

    EOSLIB_SERIALIZE( stake_t, (recipient)(stake_id)(plan_id)(staked_amount)(redeemed_amount)
                                (created_at)(updated_at) )
};


} }