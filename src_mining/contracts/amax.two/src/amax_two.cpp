
#include <amax.token.hpp>
#include "amax_two.hpp"
#include "utils.hpp"
#include <cmath>
#include <chrono>
#include <amax.token/amax.token.hpp>
#include <aplink.token/aplink.token.hpp>


using std::chrono::system_clock;
using namespace wasm;

static constexpr eosio::name active_permission{"active"_n};

// transfer out from contract self
#define TRANSFER_OUT(token_contract, to, quantity, memo) token::transfer_action(                                \
                                                             token_contract, {{get_self(), active_permission}}) \
                                                             .send(                                             \
                                                                 get_self(), to, quantity, memo);

void amax_two::init(const name& admin, const name& mine_token_contract, time_point_sec started_at, time_point_sec ended_at) {
    require_auth( _self );
    CHECKC( is_account(admin), err::ACCOUNT_INVALID, "admin not exits" );
	  CHECKC( ended_at > started_at, err::PARAM_ERROR, "end time must be greater than start time" );

    _gstate.admin                   = admin;
    _gstate.mine_token_contract     = mine_token_contract;
    _gstate.started_at              = started_at;
    _gstate.ended_at                = ended_at;
}

void amax_two::setminetoken(const asset& mine_token_total, const asset& mine_token_remained) {
    require_auth( _self );
    CHECKC( mine_token_total.symbol == SYS_SYMBOL && mine_token_remained.symbol == SYS_SYMBOL, err::PARAM_ERROR, "mine symbol must be system symbol" );

    _gstate.mine_token_total     = mine_token_total;
    _gstate.mine_token_remained  = mine_token_remained;
}

void amax_two::ontransfer(name from, name to, asset quantity, string memo) {
    if (from == get_self() || to != get_self()) 
        return;
    
    name contract = get_first_receiver();
    if (contract == APL_CONTRACT) {
        if(amax::token::is_blacklisted("amax.token"_n, from)) return;
        CHECKC( time_point_sec(current_time_point()) >= _gstate.started_at, err::NOT_STARTED, "amax #2 not open yet" )
        CHECKC( time_point_sec(current_time_point()) <  _gstate.ended_at, two_err::FINISHED, "amax #2 already ended" )
        CHECKC( quantity.symbol == APL_SYMBOL, err::SYMBOL_MISMATCH, "symbol mismatch"  )

        _claim_reward(from, quantity, "");

    } else if (contract == _gstate.mine_token_contract) {
        CHECKC( quantity.symbol == SYS_SYMBOL, err::SYMBOL_MISMATCH, "symbol mismatch"  )
        CHECKC( from == _gstate.admin, err::NO_AUTH, "no auth for operate"  )
        _gstate.mine_token_total           += quantity;
        _gstate.mine_token_remained        += quantity;
    } 
}

void amax_two::aplswaplog( const name& miner, const asset& recd_apls, const asset& swap_tokens, const name& phases, const time_point& created_at) {
    require_auth(get_self());
    require_recipient(miner);
}

void amax_two::_claim_reward( const name& from, 
                                const asset& recd_apls,
                                const string& memo )
{
    asset reward = asset(0, SYS_SYMBOL);
    _cal_reward(reward, from, recd_apls);
    CHECKC( reward <= _gstate.mine_token_remained, two_err::REWARD_NOT_ENOUGH, "reward token insufficient" )
    _gstate.mine_token_remained = _gstate.mine_token_remained - reward;

    TRANSFER(_gstate.mine_token_contract, from, reward, memo )
    _on_apl_swap_log(from, recd_apls, reward, PHASES, current_time_point());
}

void amax_two::_cal_reward( asset&   reward, 
                            const name&   from,
                            const asset&  recd_apls )
{
    asset sumbalance = aplink::token::get_sum( APL_CONTRACT, from, APL_SYMBOL.code() );  
    CHECKC( sumbalance.amount >= 1000'0000, two_err::SBT_NOT_ENOUGH, "sbt must be at least 1000" )
    double sbt =  sumbalance.amount/PERCENT_BOOST;
    double a = 1 + pow(log(sbt - 800)/16, 4.0);
    int64_t amount = a * (double(recd_apls.amount) / PERCENT_BOOST / 400.0) * AMAX_PRECISION;
    reward.set_amount(amount);
}

void amax_two::_on_apl_swap_log(
                    const name&         miner,
                    const asset&        recd_apls,
                    const asset&        swap_tokens,
                    const name&         phases,
                    const time_point&   created_at) {
    amax_two::aplswaplog_action act{ _self, { {_self, active_permission} } };
    act.send( miner, recd_apls, swap_tokens, phases, created_at );
}