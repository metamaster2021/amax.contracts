
#include <amax.token.hpp>
#include "amax_ido.hpp"
#include "utils.hpp"
#include "math.hpp"

#include <chrono>

using std::chrono::system_clock;
using namespace wasm;

inline int64_t get_precision(const symbol &s) {
    int64_t digit = s.precision();
    CHECK(digit >= 0 && digit <= 18, "precision digit " + std::to_string(digit) + " should be in range[0,18]");
    return calc_precision(digit);
}

inline int64_t get_precision(const asset &a) {
    return get_precision(a.symbol);
}

[[eosio::action]]
void amax_ido::init( const name& admin ) {
  require_auth( _self );

  _gstate.admin = admin;
  
}


[[eosio::action]]
void amax_ido::setprice(const asset &price) {
    require_auth( _gstate.admin );

    CHECK( price.symbol == USDT_SYMBOL, "Only USDT is supported for payment" )
    CHECK( price.amount > 0, "negative price not allowed" )

    _gstate.amax_price      = price;
    
}

void amax_ido::ontransfer(name from, name to, asset quantity, string memo) {
    if (from == get_self() || to != get_self()) return;

    auto first_contract = get_first_receiver();
    if (first_contract == SYS_BANK) return; //refuel only

	CHECK( quantity.amount > 0, "quantity must be positive" )
    CHECK( first_contract == USDT_BANK, "None USDT contract not allowed: " + first_contract.to_string() )
    CHECK( quantity.symbol == USDT_SYMBOL, "None USDT symbol not allowed: " + quantity.to_string() )

    auto amount     = wasm::safemath::div(quantity.amount * 100, _gstate.amax_price.amount, get_precision(_gstate.amax_price));
    auto quant      = asset(amount, SYS_SYMBOL);

    auto balance    = eosio::token::get_balance(SYS_BANK, _self, SYS_SYMBOL.code());
    CHECK( quant < balance, "insufficent funds to buy" )
    CHECK( quant >= _gstate.min_buy_amount, "buy amount too small: " + quant.to_string() )

    TRANSFER( SYS_BANK, from, quant, "ido price: " + _gstate.amax_price.to_string() )
}