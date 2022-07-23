#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include "utils.hpp"

using namespace eosio;
using namespace std;
using std::string;

// using namespace wasm;
#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

static constexpr eosio::name active_perm        {"active"_n};
static constexpr symbol SYS_SYMBOL              = SYMBOL("AMAX", 8);
static constexpr symbol USDT_SYMBOL             = SYMBOL("MUSDT", 6);
static constexpr name SYS_BANK                  { "amax.token"_n };
static constexpr name USDT_BANK                 { "amax.mtoken"_n };

namespace wasm { namespace db {

struct [[eosio::table("global"), eosio::contract("amax.ido")]] global_t {
    asset   amax_price          = asset_from_string("100.000000 MUSDT");
    asset   min_buy_amount      = asset_from_string("0.01000000 AMAX");
    // name    admin               = "armoniaadmin"_n;
    name admin                  = "amax1oomusdt"_n;

    EOSLIB_SERIALIZE( global_t, (amax_price)(min_buy_amount)(admin) )

    // //write op
    // template<typename DataStream>
    // friend DataStream& operator << ( DataStream& ds, const global_t& t ) {
    //     return ds   << t.amax_price
    //                 << t.min_buy_amount
    //                 << t.admin;
    // }
    
    // //read op (read as is)
    // template<typename DataStream>
    // friend DataStream& operator >> ( DataStream& ds, global_t& t ) {  
    //     return ds;
    // }

};
typedef eosio::singleton< "global"_n, global_t > global_singleton;


} }