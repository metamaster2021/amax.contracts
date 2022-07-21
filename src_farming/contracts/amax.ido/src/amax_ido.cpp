
#include <amax.token.hpp>
#include "amax_ido.hpp"
#include "utils.hpp"

#include <chrono>

using std::chrono::system_clock;
using namespace wasm;

static constexpr eosio::name active_permission{"active"_n};

// transfer out from contract self
#define TRANSFER_OUT(token_contract, to, quantity, memo) \
        token::transfer_action( \
            token_contract, {{_self, active_permission}}) \
            .send( _self, to, quantity, memo);

// [[eosio::action]]
// void amax_ido::init() {
//     auto issues = issue_t::tbl_t(_self, _self.value);
//     auto itr = issues.begin();
//     while (itr != issues.end()) {
//         issues.modify( *itr, _self, [&]( auto& row ) {
//         });
//     }
// }


[[eosio::action]]
void amax_ido::setprice(const asset &price) {
    require_auth( _gstate.admin );

    CHECK( price.symbol == USDT_SYMBOL, "Only USDT is supported for payment" )
    CHECK( price.amount > 0, "negative price not allowed" )

    _gstate.amax_price      = price;
    
}

[[eosio::action]]
void amax_ido::ontransfer(name from, name to, asset quantity, string memo) {
    if (from == get_self() || to != get_self()) return;

    auto first_contract = get_first_receiver();
    if (first_contract == SYS_BANK) return; //refuel only

	CHECK( quantity.amount > 0, "quantity must be positive" )
    CHECK( first_contract == USDT_BANK, "none USDT payment not allowed: " + first_contract.to_string() )

    auto amount     = 1'0000'0000 * quantity / _gstate.amax_price;
    auto quant      = asset(amount, SYS_SYMBOL);

    auto balance    = eosio::token::get_balance(SYS_BANK, _self, SYS_SYMBOL.code());
    CHECK( quant < balance, "insufficent funds to buy" )

    TRANSFER_OUT( SYS_BANK, from, quant, "ido price: " + _gstate.amax_price.to_string() )
}