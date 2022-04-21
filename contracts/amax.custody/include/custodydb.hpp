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
#warning "DAY_SECONDS_FOR_TEST should use only for test!!!"
static constexpr uint64_t DAY_SECONDS           = DAY_SECONDS_FOR_TEST;
#endif//DAY_SECONDS_FOR_TEST

static constexpr uint32_t MAX_TITLE_SIZE        = 64;


namespace wasm { namespace db {

#define CUSTODY_TBL [[eosio::table, eosio::contract("amax.custody")]]
#define CUSTODY_TBL_NAME(name) [[eosio::table(name), eosio::contract("amax.custody")]]

struct CUSTODY_TBL_NAME("global") global_t {
    bool initialized        = false;
    uint64_t trx_max_step   = 30;

    EOSLIB_SERIALIZE( global_t, (initialized)(trx_max_step) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

/* (unlock_days ->  Pair(accmulated_unlock_ratio% * 10000, proposal_txid) */
// typedef std::map<uint16_t, pair<uint16_t, checksum256>> unlock_plan_map;

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

    uint64_t by_updateat() const { return updated_at.sec_since_epoch(); }

    typedef eosio::multi_index<"plans"_n, plan_t,
        indexed_by<"updateat"_n,     const_mem_fun<plan_t, uint64_t, &plan_t::by_updateat>>,
        indexed_by<"owneridx"_n,  const_mem_fun<plan_t, uint64_t, &plan_t::by_owner> >
    > tbl_t;

    EOSLIB_SERIALIZE( plan_t, (id)(owner)(title)(asset_contract)(asset_symbol)(unlock_interval_days)(unlock_times)
                              (total_issued)(total_unlocked)(total_refunded)(enabled)(created_at)(updated_at) )

};

enum issue_status_t {
    ISSUE_NONE          = 0,
    ISSUE_UNACTIVATED   = 1,
    ISSUE_ACTIVATED     = 2,
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
    uint8_t       status = ISSUE_UNACTIVATED;   //status of issue, see issue_status_t
    time_point    issued_at;                    //issue time (UTC time)
    time_point    updated_at;                   //update time: last unlocked at

    uint64_t       scope() const { return plan_id; }
    uint64_t primary_key() const { return issue_id; }

    issue_t() {}
    issue_t(uint64_t p, uint64_t s): plan_id(p), issue_id(s) {}

    uint64_t by_updateat() const { return updated_at.sec_since_epoch(); }

    typedef eosio::multi_index<"issues"_n, issue_t,
        indexed_by<"updateat"_n,     const_mem_fun<issue_t, uint64_t, &issue_t::by_updateat>>
    > tbl_t;

    EOSLIB_SERIALIZE( issue_t,  (plan_id)(issue_id)(issuer)(receiver)(issued)(locked)(unlocked)
                                (first_unlock_days)(status)(issued_at)(updated_at) )
};

template<name::raw TableName, typename T, typename... Indices>
class multi_index_ex: public eosio::multi_index<TableName, T, Indices...> {
public:
    using base = eosio::multi_index<TableName, T, Indices...>;
    using base::base;

    template<typename Lambda>
    void set(uint64_t pk, name payer, Lambda&& setter ) {
        auto itr = base::find(pk);
        if (itr != base::end()) {
            base::emplace(payer, setter);
        } else {
            base::modify(itr, payer, setter);
        }
    }
};

struct CUSTODY_TBL account {
    // scope = contract self
    name    owner;
    uint64_t last_issue_id;

    uint64_t primary_key()const { return owner.value; }

    typedef multi_index_ex< "accounts"_n, account > tbl_t;

    EOSLIB_SERIALIZE( account,  (owner)(last_issue_id) )
};

} }