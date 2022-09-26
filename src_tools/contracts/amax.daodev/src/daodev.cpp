
#include <amax.token.hpp>
#include "daodev.hpp"

#include <amax.system/amax.system.hpp>

#include <chrono>

using namespace wasm;
using namespace eosiosystem;

static constexpr symbol   AMAX   = symbol(symbol_code("AMAX"), 8);

static constexpr name MIRROR_BANK   = name("amax.mtoken");
static constexpr name AMAX_BANK     = name("amax.token");
static constexpr name CNYD_BANK     = name("cnyd.token");

void daodev::init() {
    // check(false, "not allowed");
    require_auth( _self );

    _gstate.admin = "armoniaadmin"_n;
}

void daodev::ontransfer(const name& from, const name& to, const asset& quant, const string& memo) {
    if(_self == from || to != _self) return;

    check(quant.amount > 0, "must transfer positive quantity: " + quant.to_string());

    if (from == "amax.xchain"_n) {
        auto fund = quant * _gstate.xchain_conf.xchain_fund_pct;
        TRANSFER( MIRROR_BANK, _gstate.xchain_conf.xchain_fund_account, fund, "" )

        auto fee = quant - fund;
        TRANSFER( MIRROR_BANK, _gstate.xchain_conf.xchain_fee_account, fee, "" )
    }
}