
#include <amax.token.hpp>
#include "daodev.hpp"

#include <amax.system/amax.system.hpp>

#include <chrono>

using namespace wasm;
using namespace eosiosystem;

static constexpr name AMAX_BANK  = "amax.token"_n;
static constexpr symbol   AMAX   = symbol(symbol_code("AMAX"), 8);

static constexpr eosio::name active_permission{"active"_n};
using undelegatebw_action = eosio::action_wrapper<"undelegatebw"_n, &system_contract::undelegatebw>;

#define FORCE_TRANSFER(bank, from, to, quantity, memo) \
    	    token::transfer_action( bank, {{_self, active_perm}})\
            .send( from, to, quantity, memo );

#define UNDELEGATE_BW(from, receiver, unstate_net, unstate_cpu) \
            system_contract::undelegatebw_action( "amax"_n, {{get_self(), active_permission}}) \
            .send( from, receiver, unstate_net, unstate_cpu );

#define SELL_RAM(from, rambytes) \
            system_contract::sellram_action( "amax"_n, {{get_self(), active_permission}}) \
            .send( from, rambytes );

[[eosio::action]]
void daodev::init() {

}

