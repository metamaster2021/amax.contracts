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


enum class err: uint8_t {
   NONE                 = 0,
   RECORD_NOT_FOUND     = 1,
   RECORD_EXISTING      = 2,
   SYMBOL_MISMATCH      = 4,
   PARAM_ERROR          = 5,
   MEMO_FORMAT_ERROR    = 6,
   PAUSED               = 7,
   NO_AUTH              = 8,
   NOT_POSITIVE         = 9,
   NOT_STARTED          = 10,
   OVERSIZED            = 11,
   TIME_EXPIRED         = 12,
   NOTIFY_UNRELATED     = 13,
   ACTION_REDUNDANT     = 14,
   ACCOUNT_INVALID      = 15,
   FEE_INSUFFICIENT     = 16,
   FIRST_CREATOR        = 17,
   STATUS_ERROR         = 18,
   RATE_OVERLOAD        = 19,
   DATA_MISMATCH        = 20,
   MISC                 = 255
};

namespace split_type {
    static constexpr eosio::name AUTO             = "auto"_n;
    static constexpr eosio::name MANUAL           = "manual"_n;
};
namespace action_name {
    static constexpr uint64_t FEE           = "fee"_n.value;
    static constexpr uint64_t PLAN          = "plan"_n.value;
};

namespace plan_status {
    static constexpr name NONE              = "none"_n;
    static constexpr name RUNNING           = "running"_n;
    static constexpr name CLOSED            = "closed"_n;
};

NTBL("global") global_t {
    eosio::name admin = "armoniaadmin"_n;
    uint64_t    last_plan_id; 

    EOSLIB_SERIALIZE( global_t, (admin)(last_plan_id) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

NTBL("global2") global2_t {
    name                    fee_receiver = "armoniaadmin"_n;
    asset                   fee = asset(100000000,SYS_SYMB);
    bool                    running = true;
    uint64_t                min_split_count = 2;
    uint64_t                max_split_count = 10;
};
typedef eosio::singleton< "global2"_n, global2_t > global2_singleton;

struct split_unit_s {
    eosio::name token_receiver;
    uint64_t token_split_amount; //rate or amount, amount must contain precision
};

struct plan_trace_t{
    uint64_t plan_id;
    name issuer;
    name receiver;
    name contract;
    asset base_quantity;
    asset divide_quantity;
    uint64_t rate;
};

//scope: sender account (usually a smart contract)
//scope: _self
// new version
TBL token_split_plan_t {
    uint64_t                    id;        //PK
    name                        creator;
    symbol                      token_symbol;
    bool                        split_by_rate   = true;  //rate boost by 10000
    std::vector<split_unit_s>   split_conf;     //receiver -> rate_or_amount
    name                        split_type;     // auto or manual
    asset                       paid_fee;       
    string                      title;
    time_point_sec              create_at;
    name                        status = plan_status::NONE;

    uint64_t primary_key()const { return id; }
    uint64_t by_creator() const { return creator.value;}

    typedef multi_index<"splitplans"_n, token_split_plan_t ,
    indexed_by<"creatoridx"_n,  const_mem_fun<token_split_plan_t, uint64_t, &token_split_plan_t::by_creator> > 
    > tbl_t;
};

// scope: self
TBL account_t{
    name                        owner;
    asset                       balance = asset(0,SYS_SYMB);

    uint64_t primary_key()const { return owner.value; }

    typedef multi_index<"accounts"_n, account_t > tbl_t;
};

// scope: plan_id
TBL wallet_t {

    name                                owner;
    map<extended_symbol, asset>         balances;
    time_point_sec                      create_at;
    time_point_sec                      update_at;

    uint64_t primary_key()  const { return owner.value; }

    typedef multi_index<"wallets"_n, wallet_t
    > tbl_t;
};

} //namespace amax