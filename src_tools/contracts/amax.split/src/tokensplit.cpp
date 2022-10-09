
#include <amax.token.hpp>
#include "safemath.hpp"
#include "tokensplit.hpp"
#include "utils.hpp"

#include <chrono>

namespace amax {

using namespace wasm;
using namespace wasm::safemath;
// using namespace eosiosystem;

inline int64_t get_precision(const symbol &s) {
    int64_t digit = s.precision();
    check(digit >= 0 && digit <= 18, "precision digit " + std::to_string(digit) + " should be in range[0,18]");
    return calc_precision(digit);
}

inline int64_t get_precision(const asset &a) {
    return get_precision(a.symbol);
}

void tokensplit::init() {
    // check(false, "not allowed");
    require_auth( _self );

    // _global.remove();
}

void tokensplit::addplan(const name& plan_sender_contract, const symbol& token_symbol, const bool& split_by_rate) {
    require_auth( _gstate.admin );

    auto plan = split_plan_t( ++_gstate.last_plan_id );
    CHECK( !_db.get( plan_sender_contract.value, plan ), "plan already exists!" )

    plan.token_symbol = token_symbol;
    plan.split_by_rate = split_by_rate;

    _db.set( plan_sender_contract.value, plan, false );

}

void tokensplit::setplan(const name& plan_sender_contract, const uint64_t& plan_id, const vector<split_unit_s>& conf) {
    require_auth( _gstate.admin );

    auto plan = split_plan_t( plan_id );
    CHECK( _db.get( plan_sender_contract.value, plan ), "plan not found!" )

    plan.split_conf = conf;

    _db.set( plan_sender_contract.value, plan );

}

void tokensplit::delplan(const name& plan_sender_contract, const uint64_t& plan_id) {
    require_auth( _gstate.admin );

    auto plan = split_plan_t( plan_id );
    CHECK( !_db.get( plan_sender_contract.value, plan ), "plan already exists!" )
    _db.del( plan_sender_contract.value, plan );

}

/**
 * @brief send nasset tokens into nftone marketplace
 *
 * @param from
 * @param to
 * @param quantity
 * @param memo: plan:$plan_id
 *
 */
void tokensplit::ontransfer(const name& from, const name& to, const asset& quant, const string& memo) {
    if(_self == from || to != _self) return;

    CHECK( quant.amount > 0, "must transfer positive quantity: " + quant.to_string() )

    auto token_bank = get_first_receiver();

    vector<string_view> memo_params = split(memo, ":");
    CHECK( memo_params.size() == 2 && memo_params[0] == "plan", "memo format err" )
    auto plan_id = to_uint64(memo_params[1], "split plan");
    
    auto split_plan = split_plan_t( plan_id );
    CHECK( _db.get( from.value, split_plan ), "split plan not found for: " + to_string( plan_id ) + "@" + from.to_string() )
    CHECK( split_plan.token_symbol == quant.symbol, "incoming symbol mismatches with split plan symbol" )
    auto split_size = split_plan.split_conf.size();
    CHECK( split_size >= 1, "split conf size must be at least 1" )

    auto current_quant = quant;
    for( size_t i = 1; i < split_size; i++ ) {
        auto to = split_plan.split_conf[i].token_receiver;
        auto amount = split_plan.split_conf[i].token_split_amount;
        auto tokens = asset( 0, quant.symbol );
        tokens.amount = ( split_plan.split_by_rate ) ? mul( quant.amount, amount, PCT_BOOST ) : mul( amount, get_precision(quant.symbol), PCT_BOOST );

        if (tokens.amount > 0) {
            TRANSFER( token_bank, to, tokens, "" )

            current_quant -= tokens;

        } else
            break;
    }

    auto last_to = split_plan.split_conf[0].token_receiver;
    if (current_quant.amount > 0)
        TRANSFER( token_bank, last_to, current_quant, "" )

}

}  //namespace amax