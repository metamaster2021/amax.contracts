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
static constexpr name APL_CONTRACT              { "aplink.token"_n };

static constexpr name SYS_BANK                  { "amax.token"_n };

static constexpr uint64_t MAX_LOCK_DAYS         = 365 * 10;

#ifndef HOUR_SECONDS_FOR_TEST
static constexpr uint64_t HOUR_SECONDS           =  60 * 60;
#else
#warning "DAY_SECONDS_FOR_TEST should be used only for test!!!"
static constexpr uint64_t HOUR_SECONDS           = HOUR_SECONDS_FOR_TEST;
#endif//HOUR_SECONDS_FOR_TEST

static constexpr uint32_t MAX_TITLE_SIZE        = 64;


namespace wasm { namespace db {

#define CUSTODY_TBL [[eosio::table, eosio::contract("amax.one")]]
#define CUSTODY_TBL_NAME(name) [[eosio::table(name), eosio::contract("amax.one")]]

struct CUSTODY_TBL_NAME("global") global_t {
    name                mine_token_contract;
    name                admin;
    uint64_t            last_order_id = 0;

    time_point_sec      started_at;                 
    time_point_sec      ended_at;  

    EOSLIB_SERIALIZE( global_t, (mine_token_contract)(admin)(last_order_id)(started_at)(ended_at) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

struct CUSTODY_TBL ads_order_t {
    uint64_t        id;
    name            miner;                   //plan owner
    string          ads_id;
    asset           recd_apls;    
    time_point_sec  created_at;                 //creation time (UTC time)
    time_point_sec  expired_at;

    uint64_t primary_key() const { return id; }
    uint64_t by_miner() const { return miner.value; }
    checksum256 by_ads_id() const { return hash(ads_id); }  //unique ads id

    typedef eosio::multi_index<"adsorder"_n, ads_order_t,
        indexed_by<"mineridx"_n,  const_mem_fun<ads_order_t, uint64_t, &ads_order_t::by_miner> >,
        indexed_by<"adsidx"_n,  const_mem_fun<ads_order_t, checksum256, &ads_order_t::by_ads_id> >
    > tbl_t;

    EOSLIB_SERIALIZE( ads_order_t, (id)(miner)(ads_id)(recd_apls)(created_at)(expired_at) )

};

struct CUSTODY_TBL swap_conf_t {
    uint64_t      swap_amount;                    //PK, unique within the contract
    asset         swap_tokens;              
    asset         swap_tokens_after_adscheck;     
    asset         mine_token_total;             
    asset         mine_token_remained;            

    uint64_t primary_key() const { return swap_amount; }

    typedef eosio::multi_index<"swapconfs"_n, swap_conf_t> tbl_t;

    EOSLIB_SERIALIZE( swap_conf_t, (swap_amount)(swap_tokens)(swap_tokens_after_adscheck)
                                    (mine_token_total)(mine_token_remained))
};

} }