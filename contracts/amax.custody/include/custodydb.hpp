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

static constexpr uint64_t MAX_LOCK_DAYS         = 365 * 10;

#ifndef DAY_SECONDS_FOR_TEST
static constexpr uint64_t DAY_SECONDS           = 24 * 60 * 60;
#else
#warning "DAY_SECONDS_FOR_TEST should be used only for test!!!"
static constexpr uint64_t DAY_SECONDS           = DAY_SECONDS_FOR_TEST;
#endif//DAY_SECONDS_FOR_TEST

static constexpr uint32_t MAX_TITLE_SIZE        = 64;


namespace wasm { namespace db {

#define CUSTODY_TBL [[eosio::table, eosio::contract("amax.custody")]]
#define CUSTODY_TBL_NAME(name) [[eosio::table(name), eosio::contract("amax.custody")]]

struct CUSTODY_TBL_NAME("global") global_t {
    asset plan_fee          = asset(0, SYS_SYMBOL);
    name fee_receiver;

    EOSLIB_SERIALIZE( global_t, (plan_fee)(fee_receiver) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;


enum plan_status_t {
    PLAN_NONE          = 0,
    PLAN_UNPAID_FEE    = 1,
    PLAN_ENABLED       = 2,
    PLAN_DISABLED      = 3
};

struct CUSTODY_TBL plan_t {
    uint64_t        id;
    name            owner;                      //plan owner
    string          title;                      //plan title: <=64 chars
    name            asset_contract;             //asset issuing contract (ARC20)
    symbol          asset_symbol;               //E.g. AMAX | CNYD
    uint64_t        unlock_interval_days;       //interval between two consecutive unlock timepoints
    uint64_t        unlock_times;               //unlock times, duration=unlock_interval_days*unlock_times
    uint64_t        total_issued = 0;           //stats: updated upon issue deposit
    uint64_t        total_unlocked = 0;         //stats: updated upon unlock and endissue
    uint64_t        total_refunded = 0;         //stats: updated upon and endissue
    uint8_t         status = PLAN_UNPAID_FEE;   //status, see plan_status_t
    time_point      created_at;                 //creation time (UTC time)
    time_point      updated_at;                 //update time: last updated at

    uint64_t primary_key() const { return id; }

    uint128_t by_owner() const { return (uint128_t)owner.value << 64 | (uint128_t)id; }

    typedef eosio::multi_index<"plans"_n, plan_t,
        indexed_by<"owneridx"_n,  const_mem_fun<plan_t, uint128_t, &plan_t::by_owner> >
    > tbl_t;

    EOSLIB_SERIALIZE( plan_t, (id)(owner)(title)(asset_contract)(asset_symbol)(unlock_interval_days)(unlock_times)
                              (total_issued)(total_unlocked)(total_refunded)(status)(created_at)(updated_at) )

};

enum issue_status_t {
    ISSUE_NONE          = 0,
    ISSUE_UNDEPOSITED   = 1,
    ISSUE_NORMAL        = 2,
    ISSUE_ENDED         = 3
};

struct CUSTODY_TBL issue_t {
    // scope = contract self
    uint64_t      issue_id = 0;                 //PK, unique within the contract
    uint64_t      plan_id = 0;                  //plan id
    name          issuer;                       //issuer
    name          receiver;                     //receiver of issue who can unlock
    uint64_t      issued = 0;                   //originally issued amount
    uint64_t      locked = 0;                   //currently locked amount
    uint64_t      unlocked = 0;                 //currently unlocked amount
    uint64_t      first_unlock_days = 0;        //unlock since issued_at
    uint8_t       status = ISSUE_UNDEPOSITED;   //status of issue, see issue_status_t
    time_point    issued_at;                    //issue time (UTC time)
    time_point    updated_at;                   //update time: last unlocked at

    uint64_t primary_key() const { return issue_id; }

    uint128_t by_plan() const { return (uint128_t)plan_id << 64 | (uint128_t)issue_id; }
    uint128_t by_receiver() const { return (uint128_t)receiver.value << 64 | (uint128_t)issue_id; }
    checksum256 by_planreceiver() const {
        // same as: BigNum plan_id << 192 | receiver.value << 128 | issue_id
        return checksum256::make_from_word_sequence(0, plan_id, receiver.value, issue_id);
    }

    typedef eosio::multi_index<"issues"_n, issue_t,
        indexed_by<"planidx"_n,     const_mem_fun<issue_t, uint128_t, &issue_t::by_plan>>,
        indexed_by<"receiveridx"_n,     const_mem_fun<issue_t, uint128_t, &issue_t::by_receiver>>
        indexed_by<"planreceiver"_n,     const_mem_fun<issue_t, checksum256, &issue_t::by_planreceiver>>
    > tbl_t;

    EOSLIB_SERIALIZE( issue_t,  (issue_id)(plan_id)(issuer)(receiver)(issued)(locked)(unlocked)
                                (first_unlock_days)(status)(issued_at)(updated_at) )
};

struct CUSTODY_TBL account {
    // scope = contract self
    name    owner;
    uint64_t last_plan_id;
    uint64_t last_issue_id;

    uint64_t primary_key()const { return owner.value; }

    typedef multi_index_ex< "accounts"_n, account > tbl_t;

    EOSLIB_SERIALIZE( account,  (owner)(last_plan_id)(last_issue_id) )
};

} }