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

namespace amax {

using namespace std;
using namespace eosio;

#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

static constexpr eosio::name active_perm{"active"_n};
static constexpr eosio::name CNYD_BANK{"cnyd.token"_n};

static constexpr uint64_t percent_boost     = 10000;
static constexpr uint64_t max_memo_size     = 1024;

#define hash(str) sha256(const_cast<char*>(str.c_str()), str.size());

namespace chain {
    static constexpr eosio::name BTC         = "btc"_n;
    static constexpr eosio::name ETH         = "eth"_n;
    static constexpr eosio::name AMC         = "amc"_n;
    static constexpr eosio::name BSC         = "bsc"_n;
    static constexpr eosio::name HECO        = "heco"_n;
    static constexpr eosio::name POLYGON     = "polygon"_n;
    static constexpr eosio::name TRON        = "tron"_n;
    static constexpr eosio::name EOS         = "eos"_n;
};

#define TBL struct [[eosio::table, eosio::contract("amax.xchain")]]

struct [[eosio::table("global"), eosio::contract("amax.xchain")]] global_t {
    name admin;                 // default is contract self
    name maker;
    name checker;
    name fee_collector;         // mgmt fees to collector
    uint64_t fee_rate = 4;      // boost by 10,000, i.e. 0.04%
    bool active = false;

    map<symbol_code, vector<name>> xchain_assets = {
        { symbol_code("AMBTC"),  { chain::BTC } },
        { symbol_code("AMETH"),  { chain::ETH } },
        { symbol_code("AMUSDT"), { chain::ETH, chain::BSC, chain::TRON } }
    };

    EOSLIB_SERIALIZE( global_t, (admin)(maker)(checker)(fee_collector)(fee_rate)(active)(xchain_assets) )
};

typedef eosio::singleton< "global"_n, global_t > global_singleton;

// enum class order_status: name {
//     CREATED         = "created"_n;
//     FUFILLED        = "fufilled"_n;
//     CANCELED        = "canceled"_n;
// };

// enum class address_status: name {
//     PENDING         = "pending"_n,
//     INITIALIZED     = "initialize"_n
// };

///cross-chain deposit address
///Scope: account
struct account_xchain_address_t {
    uint64_t        id;
    name            account;
    name            base_chain; 
    string          xin_to;            //E.g. Eth or BTC address, eos id
    name            status;

    time_point_sec  created_at;
    time_point_sec  updated_at;

    account_xchain_address_t() {};
    account_xchain_address_t(const name& ch): base_chain(ch) {};


    uint64_t primary_key()const { return id; }

    uint64_t by_update_time() const {
        return (uint64_t) updated_at.utc_seconds ;
    }

    checksum256 by_xin_to() const {
        return hash(xin_to); 
    }

    typedef eosio::multi_index<"xinaddrmap"_n, account_xchain_address_t,
        indexed_by<"updatedat"_n, const_mem_fun<account_xchain_address_t, uint64_t, &account_xchain_address_t::by_update_time> >
    > idx_t;

    EOSLIB_SERIALIZE( account_xchain_address_t, (id)(account)(base_chain)(xin_to)(status)(created_at)(updated_at) )
};

TBL xin_order_t {
    uint64_t        id;         //PK
    string          txid;
    name            chain;
    name            status; //xin_order_status
    asset           submitted;  //for deposit_quantity
    asset           verified;   //for deposit_quantity
    name            maker;
    name            checker;
    time_point_sec  submitted_at;
    time_point_sec  verified_at;

    xin_order_t() {}
    xin_order_t(const uint64_t& i): id(i) {}

    uint64_t    primary_key()const { return id; }
    uint64_t    by_chain() const { return chain.value; }
    checksum256 by_txid() const { return hash(txid); }    //unique index
    uint64_t    by_status() const { return status.value; }

    typedef eosio::multi_index
      < "xinorders"_n,  xin_order_t,
        indexed_by<"xintxids"_n, const_mem_fun<xin_order_t, checksum256, &xin_order_t::by_txid> >,
        indexed_by<"xinstatus"_n, const_mem_fun<xin_order_t, uint64_t, &xin_order_t::by_status> >
    > idx_t;

    EOSLIB_SERIALIZE(xin_order_t, (id)(txid)(chain)(status)(submitted)(verified)(maker)(checker)(submitted_at)(verified_at) )

};

TBL xout_order_t {
    uint64_t        id;         //PK
    string          txid;
    name            chain;
    name            status;     //xout_order_status
    asset           submitted;  //for deposit_quantity
    asset           verified;   //for deposit_quantity
    name            maker;
    name            checker;
    time_point_sec  submitted_at;
    time_point_sec  verified_at;

    xout_order_t() {};

    checksum256 by_txid() const { return hash(txid); }    //unique index

    typedef eosio::multi_index
      < "xoutorders"_n,  xout_order_t,
        indexed_by<"xouttxids"_n, const_mem_fun<xin_order_t, checksum256, &xin_order_t::by_txid> >
    > idx_t;

    uint64_t primary_key() const { return id; };

    EOSLIB_SERIALIZE(xout_order_t, (id)(txid)(chain) )
};

} // amax
