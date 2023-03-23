
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

void amax_two::init(const name& admin, const name& mine_token_contract, time_point_sec started_at, time_point_sec ended_at, const asset& mine_token_total) {
    require_auth( _self );
    CHECK( is_account(admin), "admin not exits" );
	  CHECK( ended_at > started_at, "end time must be greater than start time" );
    asset value = token::get_supply(mine_token_contract, mine_token_total.symbol.code());
    CHECK( value.amount > 0, "symbol mismatch" );
    
    _gstate.admin                   = admin;
    _gstate.mine_token_contract     = mine_token_contract;
    _gstate.started_at              = started_at;
    _gstate.ended_at                = ended_at;
    _gstate.mine_token_total        = mine_token_total;
    _gstate.mine_token_remained     = mine_token_total;
}

void amax_two::ontransfer(name from, name to, asset quantity, string memo) {
    if (from == get_self() || to != get_self()) 
      return;
    if(amax::token::is_blacklisted("amax.token"_n, from))
      return;
    CHECK( time_point_sec(current_time_point()) >= _gstate.started_at, "amax #2 not open yet" )
    CHECK( time_point_sec(current_time_point()) <  _gstate.ended_at, "amax #2 already ended" )
    CHECK( quantity.symbol == APL_SYMBOL, "Ntwo APL symbol not allowed: " + quantity.to_string() )

    _claim_reward(from, quantity, "");
}

void amax_two::aplswaplog( const name& miner, const asset& recd_apls, const asset& swap_tokens, const time_point& created_at) {
    require_auth(get_self());
    require_recipient(miner);
}

void amax_two::addminetoken(const name& account, const asset& mine_token_total, const asset& mine_token_remained) 
{
    require_auth( account );
    CHECK(account == _self || account == _gstate.admin , "no auth for operate");
    _gstate.mine_token_total           += mine_token_total;
    _gstate.mine_token_remained        += mine_token_remained;
}

void amax_two::_claim_reward( const name& to, 
                                const asset& recd_apls,
                                const string& memo )
{
    asset reward = asset(0, SYS_SYMBOL);
    _cal_reward(reward, to, recd_apls);
    CHECK( reward <= _gstate.mine_token_remained, "reward token not enough" )
    _gstate.mine_token_remained = _gstate.mine_token_remained - reward;

    TRANSFER(_gstate.mine_token_contract, to, reward, memo )
    _on_apl_swap_log(to, recd_apls, reward, current_time_point());
}

void amax_two::_cal_reward( asset&   reward, 
                            const name&   to,
                            const asset&  recd_apls )
{
    asset sumbalance = aplink::token::get_sum( APL_CONTRACT, to, APL_SYMBOL.code() );  
    CHECK( sumbalance.amount >= 1000'0000, "sbt must be at least 1000" )

    double sbt =  sumbalance.amount/PERCENT_BOOST;
    double a = 1 + power(log(sbt- 800)/16, 2);
    int64_t amount = a * (recd_apls.amount / PERCENT_BOOST / 400) * AMAX_PRECISION;
    reward.set_amount(amount);
}

void amax_two::_on_apl_swap_log(
                    const name&         miner,
                    const asset&        recd_apls,
                    const asset&        swap_tokens,
                    const time_point&   created_at) {
    amax_two::aplswaplog_action act{ _self, { {_self, active_permission} } };
    act.send( miner, recd_apls, swap_tokens, created_at );
}