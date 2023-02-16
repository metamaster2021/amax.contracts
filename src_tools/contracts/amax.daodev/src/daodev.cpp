
#include <amax.token.hpp>
#include "safemath.hpp"
#include "daodev.hpp"
#include "utils.hpp"

#include <amax.system/amax.system.hpp>

#include <chrono>

using namespace wasm;
using namespace wasm::safemath;
// using namespace eosiosystem;

static constexpr symbol   AMAX   = symbol(symbol_code("AMAX"), 8);

static constexpr name MIRROR_BANK   = name("amax.mtoken");
static constexpr name AMAX_BANK     = name("amax.token");
static constexpr name CNYD_BANK     = name("cnyd.token");

static constexpr uint8_t PCT_BOOST = 100;

inline int64_t get_precision(const symbol &s) {
    int64_t digit = s.precision();
    check(digit >= 0 && digit <= 18, "precision digit " + std::to_string(digit) + " should be in range[0,18]");
    return calc_precision(digit);
}

inline int64_t get_precision(const asset &a) {
    return get_precision(a.symbol);
}

void daodev::init() {
    // check(false, "not allowed");
    require_auth( _self );

    // _global.remove();
}

void daodev::ontransfer(const name& from, const name& to, const asset& quant, const string& memo) {
    if(_self == from || to != _self) return;

    check(quant.amount > 0, "must transfer positive quantity: " + quant.to_string());

    // if (get_first_receiver())

    if (from == "amax.xchain"_n) {
        auto fund_amount = mul( quant.amount, _gstate.xchain_conf.xchain_fund_pct, PCT_BOOST );
        auto fund = asset( fund_amount, quant.symbol );
      
        TRANSFER( MIRROR_BANK, _gstate.xchain_conf.xchain_fund_account, fund, "" )

        auto fee = quant - fund;
        TRANSFER( MIRROR_BANK, _gstate.xchain_conf.xchain_fee_account, fee, "" )
    }
}