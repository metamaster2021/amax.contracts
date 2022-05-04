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

enum class chain_type: name {
    BTC         = "btc"_,
    ETH         = "eth"_,
    AMC         = "amc"_,
    BSC         = "bsc"_,
    HECO        = "heco"_,
    POLYGON     = "polygon"_,
    TRON        = "tron"_,
    EOS         = "eos"_
};

#define TBL struct [[eosio::table, eosio::contract("amax.xchain")]]

struct [[eosio::table("global"), eosio::contract("amax.xchain")]] global_t {
    name admin;                 // default is contract self
    name maker;
    name check;
    name fee_collector;         // mgmt fees to collector
    uint64_t fee_rate = 4;      // boost by 10,000, i.e. 0.04%
    bool active = false;
   

    EOSLIB_SERIALIZE( global_t, (admin)(maker)(checker)(fee_collector)(fee_rate)(active) )
};

typedef eosio::singleton< "global"_n, global_t > global_singleton;

enum class order_status: name {
    CREATED         = "created"_n,
    FUFILLED        = "fufilled"_n,
    CANCELED        = "canceled"_n
};

///cross-chain deposit address
///Scope: account
struct account_xchain_address_t {
    name            base_chain; 
    string          address;            //E.g. Eth or BTC address
    time_point_sec  created_at;
    time_point_sec  updated_at;

    account_xchain_address_t() {};
    account_xchain_address_t(const name& ch): base_chain(ch) {};

    uint64_t primary_key()const { return base_chain.value; }

    EOSLIB_SERIALIZE( account_xchain_address_t, (base_chain)(address)(created_at)(updated_at) )

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

    uint64_t    primary_key()const { return balance.symbid; }
    uint64_t    by_chain() { return chain.value; }
    checksum256 by_txid() { return hash(txid); }    //unique index
    uint64_t    by_status() { return status.value; }

    typedef eosio::multi_index
      < "xinorders"_n,  xin_order_t,
        indexed_by<"xintxids"_n, const_mem_fun<xin_order_t, uint64_t, &xin_order_t::by_txid> >
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
    xout_order_t(const uint64_t& id): symbid(id) {};

    uint64_t primary_key()const { return symbid; }

    typedef eosio::multi_index< "tokenstats"_n, xout_order_t > idx_t;

    EOSLIB_SERIALIZE(xout_order_t, (symbid)(type)(uri)(max_supply)(supply)(issuer)(created_at)(paused) )
};

} // amax
