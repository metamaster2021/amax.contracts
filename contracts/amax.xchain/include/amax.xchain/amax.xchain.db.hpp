 #pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include <deque>
#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>

namespace apollo {

using namespace std;
using namespace eosio;

#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

static constexpr eosio::name active_perm{"active"_n};
static constexpr eosio::name CNYD_BANK{"cnyd.token"_n};

static constexpr uint64_t percent_boost     = 10000;
static constexpr uint64_t max_memo_size     = 1024;

// static constexpr uint64_t seconds_per_year      = 24 * 3600 * 7 * 52;
// static constexpr uint64_t seconds_per_month     = 24 * 3600 * 30;
// static constexpr uint64_t seconds_per_week      = 24 * 3600 * 7;
// static constexpr uint64_t seconds_per_day       = 24 * 3600;
// static constexpr uint64_t seconds_per_hour      = 3600;

enum class asset_type_t : uint16_t {
    NONE                        = 0,
    POW_ASSET                   = 1,
    POS_ASSET                   = 2,
};

#define TBL struct [[eosio::table, eosio::contract("amax.xchain")]]

struct [[eosio::table("global"), eosio::contract("amax.xchain")]] global_t {
    name admin;                 // default is contract self
    name fee_collector;         // mgmt fees to collector
    uint64_t fee_rate = 4;      // boost by 10,000, i.e. 0.04%
    bool active = false;

    EOSLIB_SERIALIZE( global_t, (admin)(fee_collector)(fee_rate)(active) )
};

typedef eosio::singleton< "global"_n, global_t > global_singleton;

struct token_asset {
    uint64_t symbid;
    int64_t  amount;

    token_asset& operator+=(const token_asset& value) { this->amount += value.amount; return *this; } 
    token_asset& operator-=(const token_asset& value) { this->amount -= value.amount; return *this; }

    token_asset(){};
    token_asset(const uint64_t& id): symbid(id) {};
    token_asset(const uint64_t& id, const int64_t& q): symbid(id), amount(q){};

    EOSLIB_SERIALIZE(token_asset, (symbid)(amount) )
};

///Scope: owner's account
TBL account_t {
    token_asset     balance;
    asset           last_recd_earning;
    asset           total_recd_earing;
    time_point      last_settled_at;
    bool paused     = false;   //if true, it can no longer be transferred

    account_t() {}
    account_t(const uint64_t& symbid): balance(symbid) {}

    uint64_t    primary_key()const { return balance.symbid; }

    typedef eosio::multi_index< "accounts"_n, account_t > idx_t;

    EOSLIB_SERIALIZE(account_t, (balance)(last_recd_earning)(total_recd_earing)(last_settled_at)(paused) )

};

TBL tokenstats_t {
    uint64_t        symbid;         //PK
    uint16_t        type;           // 0: POW assets, 1: POS assets, ...etc
    string          uri;            // token_uri for token metadata { image }
    int64_t         max_supply;     // when amount is 1, it means NFT-721 type
    int64_t         supply;
    name            issuer;
    time_point_sec  created_at;
    bool paused     = false;

    tokenstats_t() {};
    tokenstats_t(const uint64_t& id): symbid(id) {};

    uint64_t primary_key()const { return symbid; }

    typedef eosio::multi_index< "tokenstats"_n, tokenstats_t > idx_t;

    EOSLIB_SERIALIZE(tokenstats_t, (symbid)(type)(uri)(max_supply)(supply)(issuer)(created_at)(paused) )
};

struct hashrate_t {
    float value;
    char unit;
};

struct pow_asset_meta {
    string product_sn;                              //product serial no
    string manufacturer;                            //manufacture info
    string mine_coin_type;                          //btc, eth
    hashrate_t hashrate;                            //hash_rate and hash_rate_unit(M/T) E.g. 21.457 MH/s
    float power_in_watt;                            //E.g. 2100 Watt
    asset electricity_day_charge;                   //E.g. "0.85 CNYD" for reference
    asset daily_earning_est;                        //daily earning estimate: E.g. "0.00397002 AMETH"
    uint16_t service_life_days;                     //service lifespan (E.g. 3*365)
    uint8_t onshelf_days;                           //0: T+0, 1:T+1

    pow_asset_meta() {};

    EOSLIB_SERIALIZE(pow_asset_meta, (product_sn)(manufacturer)(mine_coin_type)(hashrate)(power_in_watt)
                                    (electricity_day_charge)(daily_earning_est)(service_life_days)(onshelf_days) )
};

/**
 * POW Mining equipment asset
 * 
 */
TBL pow_asset_t {
    uint64_t symbid;
    pow_asset_meta asset_meta;
    
    pow_asset_t() {}
    pow_asset_t(const uint64_t& id): symbid(id) {}

    uint64_t primary_key()const { return symbid; }

    // uint64_t by_owner()const { return owner.value; }
    uint64_t by_mine_coin()const { return name(asset_meta.mine_coin_type).value; }
    // uint64_t by_started_at()const { return started_at.sec_since_epoch(); }

    //unique index
    // uint128_t by_owner_symbid()const { return (uint128_t) owner.value << 64 | (uint128_t) symbid; }
  
    EOSLIB_SERIALIZE(pow_asset_t, (symbid)(asset_meta) )

    typedef eosio::multi_index
    < "powassets"_n,  pow_asset_t,
        // indexed_by<"owner"_n, const_mem_fun<pow_asset_t, uint64_t, &pow_asset_t::by_owner> >,
        indexed_by<"minecoin"_n, const_mem_fun<pow_asset_t, uint64_t, &pow_asset_t::by_mine_coin> >
        // indexed_by<"startedat"_n, const_mem_fun<pow_asset_t, uint64_t, &pow_asset_t::by_started_at> >,
        //indexed_by<"ownersymbid"_n, const_mem_fun<pow_asset_t, uint128_t, &pow_asset_t::by_owner_symbid> >
    > idx_t;
};


} // apollo
