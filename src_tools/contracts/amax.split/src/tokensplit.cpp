
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

    check(quant.amount > 0, "must transfer positive quantity: " + quant.to_string());

    auto token_bank = get_first_receiver();

    vector<string_view> memo_params = split(memo, ":");
    CHECK( memo_params.size() == 2 && memo_params[0] == "plan", "memo format err" )
    auto plan_id = to_uint64(memo_params[1], "split plan");
    
    auto split_plan = split_plan_t( plan_id );
    CHECK( _db.get( from.value, split_plan ), "split plan not found for: " + to_string( plan_id ) + "@" + from.to_string() )

    auto current_quant = quant;
    auto i = 0;
    for( ; i < split_plan.split_conf.size() - 1; i++ ) {
        auto to = split_plan.split_conf[i].token_receiver;
        auto amount = split_plan.split_conf[i].token_split_amount;
        auto tokens = asset( 0, quant.symbol );
        tokens.amount = ( split_plan.split_by_rate ) ? mul( quant.amount, amount, PCT_BOOST ) : amount * get_precision( quant );

        if (tokens.amount > 0) {
            TRANSFER( token_bank, to, tokens, "" )

            current_quant -= tokens;

        } else
            break;
    }

    auto last_to = split_plan.split_conf[i].token_receiver;
    if (current_quant.amount > 0)
        TRANSFER( token_bank, last_to, current_quant, "" )

}

}  //namespace amax