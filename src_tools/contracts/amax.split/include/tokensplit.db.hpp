#pragma once

#include "wasm_db.hpp"

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include <string>
#include <vector>
#include <type_traits>

namespace amax {

using namespace std;
using std::string;
using namespace eosio;

// using namespace wasm;
#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

static constexpr eosio::name active_perm        {"active"_n};
static constexpr symbol SYS_SYMB                = SYMBOL("AMAX", 8);
static constexpr name SYS_BANK                  { "amax.token"_n };

#define TBL struct [[eosio::table, eosio::contract("amax.split")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("amax.split")]]

struct xchain_conf_s {
    name xchain_fund_account        = "xchain.fund"_n;
    name xchain_fee_account         = "xchain.fee"_n;
    uint8_t xchain_fund_pct         = 60;  //60%, xchain_fee: 40%
};

NTBL("global") global_t {
    eosio::name admin = "armoniaadmin"_n;

    EOSLIB_SERIALIZE( global_t, (admin))
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

struct split_unit_s {
    eosio::name token_receiver;
    uint64_t token_split_amount; //rate or amount
};

//scope: sender account (usually a smart contract)
TBL split_plan_t {
    uint64_t                    id;        //PK
    bool                        split_by_rate   = false;  //rate boost by 10000
    std::vector<split_unit_s>   split_conf;     //receiver -> rate_or_amount

    split_plan_t() {}
    split_plan_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }

    typedef multi_index<"splitplans"_n, split_plan_t > tbl_t;

    EOSLIB_SERIALIZE( split_plan_t, (id)(split_by_rate)(split_conf) )
};

} //namespace amax