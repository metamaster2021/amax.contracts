 #pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>
#include <utils.hpp>

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
static constexpr uint64_t max_addr_len      = 128;

typedef set<symbol> symbolset;
typedef set<name> nameset;


#define hash(str) sha256(const_cast<char*>(str.c_str()), str.size())

enum class err: uint8_t {
    NONE                = 0,
    RECORD_NOT_FOUND    = 1,
    RECORD_EXISTING     = 2,
    ADDRESS_ILLEGAL     = 3,
    SYMBOL_MISMATCH     = 4,
    ADDRESS_MISMATCH    = 5,
    NOT_COMMON_XIN      = 6,
    STATUS_INCORRECT    = 7,
    PARAM_INCORRECT     = 8,
    NO_AUTH             = 9,

};

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

namespace coin {
    static constexpr eosio::name BTC        = "btc"_n;
    static constexpr eosio::name ETH        = "eth"_n;
    static constexpr eosio::name USDT       = "usdt"_n;
    static constexpr eosio::name CNYD       = "cnyd"_n;
};

#define TBL struct [[eosio::table, eosio::contract("amax.xchain")]]

struct [[eosio::table("global"), eosio::contract("amax.xchain")]] global_t {
    name admin;                 // default is contract self
    name maker;
    name checker;
    name fee_collector;         // mgmt fees to collector
    uint64_t fee_rate = 4;      // boost by 10,000, i.e. 0.04%
    bool active = false;

    EOSLIB_SERIALIZE( global_t, (admin)(maker)(checker)(fee_collector)(fee_rate)(active) )
};

typedef eosio::singleton< "global"_n, global_t > global_singleton;

namespace address_status {
    static constexpr eosio::name REQUESTED          = "requested"_n;
    static constexpr eosio::name PROVISIONED        = "provisioned"_n;
};


namespace xin_order_status {
    static constexpr eosio::name CREATED            = "created"_n;
    static constexpr eosio::name CHECKED            = "checked"_n;
    static constexpr eosio::name CANCELED           = "canceled"_n;
};

namespace xout_order_status {
    static constexpr eosio::name CREATED            = "created"_n;
    static constexpr eosio::name SENT               = "sent"_n;        // by maker after sending coins in third chain
    static constexpr eosio::name CONFIRMED          = "confirmed"_n;   // by maker after block finality confirmation.
    static constexpr eosio::name CHECKED            = "checked"_n;
    static constexpr eosio::name CANCELED           = "canceled"_n;
};

///cross-chain deposit address
TBL account_xchain_address_t {
    uint64_t        id;
    name            account;
    name            base_chain;
    uint32_t        mulsign_wallet_id;
    string          xin_to;            //E.g. Eth or BTC address, eos id
    name            status = address_status::REQUESTED;
    time_point_sec  created_at;
    time_point_sec  updated_at;

    account_xchain_address_t() {};
    account_xchain_address_t(const name& a, const name& bc, const uint64_t& wi) : account(a), base_chain(bc), mulsign_wallet_id(wi) {};


    uint64_t    primary_key()const { return id; }
    uint64_t    by_update_time() const { return (uint64_t) updated_at.utc_seconds; }
    uint128_t   by_accout_base_chain() const { return make128key( account.value, make64key( base_chain.value, mulsign_wallet_id )); }
    checksum256 by_xin_to() const { return hash(xin_to); }

    typedef eosio::multi_index<"xinaddrmap"_n, account_xchain_address_t,
        indexed_by<"updatedat"_n, const_mem_fun<account_xchain_address_t, uint64_t, &account_xchain_address_t::by_update_time> >,
        indexed_by<"acctchain"_n, const_mem_fun<account_xchain_address_t, uint128_t, &account_xchain_address_t::by_accout_base_chain> >
    > idx_t;

    EOSLIB_SERIALIZE( account_xchain_address_t, (id)(account)(base_chain)(xin_to)(status)(created_at)(updated_at) )
};

TBL xin_order_t {
    uint64_t        id;         //PK
    string          txid;
    name            account;
    uint32_t        mulsign_wallet_id;
    string          xin_from;
    string          xin_to;
    name            chain;
    symbol          coin_name;
    asset           quantity;   //for deposit_quantity
    name            status;     //xin_order_status

    string          close_reason;
    name            maker;
    name            checker;
    time_point_sec  created_at;
    time_point_sec  closed_at;
    time_point_sec  updated_at;

    xin_order_t() {}
    xin_order_t(const uint64_t& i): id(i) {}

    uint64_t    primary_key()const { return id; }
    uint64_t    by_update_time() const { return (uint64_t) updated_at.utc_seconds; }

    uint64_t    by_chain() const { return chain.value; }
    checksum256 by_txid() const { return hash(txid); }    //unique index
    uint64_t    by_status() const { return status.value; }

    typedef eosio::multi_index
      < "xinorders"_n,  xin_order_t,
        indexed_by<"updatedat"_n, const_mem_fun<xin_order_t, uint64_t, &xin_order_t::by_update_time> >,
        indexed_by<"xintxids"_n, const_mem_fun<xin_order_t, checksum256, &xin_order_t::by_txid> >,
        indexed_by<"xinstatus"_n, const_mem_fun<xin_order_t, uint64_t, &xin_order_t::by_status> >
    > idx_t;

    EOSLIB_SERIALIZE(xin_order_t,   (id)(txid)(account)(xin_from)(xin_to)
                                    (chain)(coin_name)(quantity)(status)
                                    (close_reason)(maker)(checker)(created_at)(closed_at)(updated_at) )

};

TBL xout_order_t {
    uint64_t        id;         //PK
    string          txid;
    name            account;
    string          xout_from; 
    string          xout_to;
    name            chain;
    symbol          coin_name;

    asset           apply_quantity; 
    asset           quantity;
    asset           fee;
    name            status;
    string          memo;
    
    string          close_reason;
    name            maker;
    name            checker;
    time_point_sec  created_at;
    time_point_sec  closed_at;
    time_point_sec  updated_at;

    uint64_t    primary_key()const { return id; }
    uint64_t    by_update_time() const { return (uint64_t) updated_at.utc_seconds; }
    checksum256 by_txid() const { return hash(txid); }    //unique index
    uint64_t    by_status() const { return status.value; }

    typedef eosio::multi_index
      < "xoutorders"_n,  xout_order_t,
        indexed_by<"updatedat"_n, const_mem_fun<xout_order_t, uint64_t, &xout_order_t::by_update_time> >,
        indexed_by<"xouttxids"_n, const_mem_fun<xout_order_t, checksum256, &xout_order_t::by_txid> >,
        indexed_by<"xoutstatus"_n, const_mem_fun<xout_order_t, uint64_t, &xout_order_t::by_status> >
    > idx_t;

    EOSLIB_SERIALIZE(xout_order_t,  (id)(txid)(account)(xout_from)(xout_to)(chain)(coin_name)
                                    (apply_quantity)(quantity)(fee)(status)(memo)
                                    (close_reason)(maker)(checker)(created_at)(closed_at)(updated_at) )
                                    
};

TBL chain_t {
    name        chain;         //PK
    name        base_chain;    //if base_chain is null, the chain is base chain
    string      common_xin_account = "";

    chain_t() {};
    chain_t( const name& c ):chain( c ) {}

    uint64_t primary_key()const { return chain.value; }

    typedef eosio::multi_index< "chains"_n,  chain_t > idx_t;

    EOSLIB_SERIALIZE(chain_t, (chain)(base_chain)(common_xin_account) );
};

TBL coin_t {
    symbol          coin;         //PK

    coin_t() {};
    coin_t( const symbol& c ):coin( c ) {}

    uint64_t primary_key()const { return coin.raw(); }

    typedef eosio::multi_index< "coins"_n,  coin_t > idx_t;

    EOSLIB_SERIALIZE(coin_t, (coin) );

};

TBL chain_coin_t {
    uint64_t        id;             //PK
    name            chain;          //co-PK
    symbol          coin;           //co-PK
    asset           fee;

    chain_coin_t() {};
    chain_coin_t( const name& ch, const symbol& co ):chain( ch ),coin( co ) {}

    string to_string() const { return chain.to_string() + "_" + coin.code().to_string(); } 

    uint64_t primary_key()const { return id; }
    uint128_t by_chaincoin()const { return (uint128_t) chain.value << 64 | (uint128_t)coin.code().raw();  }

    typedef eosio::multi_index< "chaincoins"_n,  chain_coin_t ,
        indexed_by<"chaincoin"_n, const_mem_fun<chain_coin_t, uint128_t, &chain_coin_t::by_chaincoin> >
     > idx_t;

    EOSLIB_SERIALIZE( chain_coin_t, (id)(chain)(coin)(fee) );

};

} // amax
