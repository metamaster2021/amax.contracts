
#include <amax.token.hpp>
#include "amax_one.hpp"
#include "utils.hpp"
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

//tcli push action $c init '['$c',"2022-07-27T09:25:00.000","2022-12-14T10:29:00.000",amax.ntoken]' -p $c

void amax_one::init(const name& admin, const name& mine_token_contract, time_point_sec started_at, time_point_sec ended_at) {
    require_auth( _self );
    _gstate.admin                   = admin;
    _gstate.mine_token_contract     = mine_token_contract;
    _gstate.started_at              = started_at;
    _gstate.ended_at                = ended_at;
}
/**
 * memo: adsid:11111
 * 
 **/
void amax_one::ontransfer(name from, name to, asset quantity, string memo) {
    if (from == get_self() || to != get_self()) return;


	  CHECK( quantity.amount > 0, "quantity must be positive" )
    asset sumbalance = aplink::token::get_sum( APL_CONTRACT, from, APL_SYMBOL.code() );  
    CHECK( sumbalance.amount < 1000'0000, "apl sum greater than or equal to 1000, please go to the pro pool to mine" )
    
    if(amax::token::is_blacklisted("amax.token"_n, from))
        return;
    CHECK( time_point_sec(current_time_point()) >=  _gstate.started_at, "amax #1 not open yet" )
    CHECK( time_point_sec(current_time_point()) <  _gstate.ended_at, "amax #1 already ended" )
    CHECK( quantity.symbol == APL_SYMBOL, "None APL symbol not allowed: " + quantity.to_string() )

    swap_conf_t::tbl_t swap_conf_tbl(get_self(), get_self().value);
    auto swap_conf_itr = swap_conf_tbl.find(quantity.amount);
    CHECK( swap_conf_itr != swap_conf_tbl.end(), "swap amount not supported: " + quantity.to_string() )
    CHECK( swap_conf_itr->mine_token_remained >=  swap_conf_itr->swap_tokens, "swap not enought amax ")

    if(memo.length() > 10) {
        string_view ads_id = _get_ads_id(memo);
        if (quantity.amount == 1000000 &&  ads_id.length() > 4) {
            _add_adsorder( from, quantity, string(ads_id) );
            return;
        }
    }

    _claim_reward(from, quantity, false, "","");
}

void amax_one::_add_adsorder(const name& miner, const asset& quantity, const string& ads_id) {
    
    ads_order_t::tbl_t ads_order_tbl(get_self(), get_self().value);

    _gstate.last_order_id++;
    ads_order_tbl.emplace(get_self(), [&](auto &order) {
        order.id            = _gstate.last_order_id;
        order.miner         = miner;
        order.ads_id        = ads_id;
        order.recd_apls     = quantity;
        order.created_at    = time_point_sec(current_time_point());
        order.expired_at    = time_point_sec(current_time_point()) + HOUR_SECONDS;
    });
}

void amax_one::confirmads( const uint64_t& order_id ) {
    
    require_auth(  _gstate.admin );
    ads_order_t::tbl_t ads_order_tbl(get_self(), get_self().value);
    auto ads_order_idx = ads_order_tbl.find(order_id);
    check(ads_order_idx != ads_order_tbl.end(), "order_id not existed");

    _claim_reward(ads_order_idx->miner, ads_order_idx->recd_apls,
                     true, ads_order_idx->ads_id, "");

    ads_order_tbl.erase(ads_order_idx);
}

void amax_one::onswapexpird( const uint64_t& order_id ) {
    require_auth(_gstate.admin);

    ads_order_t::tbl_t ads_order_tbl(get_self(), get_self().value);

    auto itr = ads_order_tbl.find(order_id);
    CHECK(itr != ads_order_tbl.end(), "order_id not existed");

    _claim_reward( itr->miner,  itr->recd_apls, false, "", "" );   
    ads_order_tbl.erase(itr);
}

 void amax_one::aplswaplog( const name& miner, const asset& recd_apls, const asset& swap_tokens, const string& ads_id, const time_point& created_at) {
    require_auth(get_self());
    require_recipient(miner);
 }

void amax_one::addswapconf(
            const name&         account,
            const uint64_t&     swap_amount,
            const asset&        swap_tokens,
            const asset&        swap_tokens_after_adscheck,
            const asset&        mine_token_total,
            const asset&        mine_token_remained) 
{
    require_auth( account );
    CHECK(account == _self || account == _gstate.admin , "no auth for operate");

    swap_conf_t::tbl_t swap_conf_tbl(get_self(), get_self().value);
    auto swap_conf_itr = swap_conf_tbl.find(swap_amount);
    CHECK( swap_conf_itr == swap_conf_tbl.end(), "swap conf already existed: " + to_string(swap_amount) )

    swap_conf_tbl.emplace( _self, [&](auto &conf) {
        conf.swap_amount                = swap_amount;
        conf.swap_tokens                = swap_tokens;
        conf.swap_tokens_after_adscheck   = swap_tokens_after_adscheck;
        conf.mine_token_total           = mine_token_total;
        conf.mine_token_remained        = mine_token_remained;
    });

}

void amax_one::setremained( const uint64_t& swap_amount, const asset& amount) {
    require_auth( _self );

    swap_conf_t::tbl_t swap_conf_tbl(get_self(), get_self().value);
    auto swap_conf_itr = swap_conf_tbl.find(swap_amount);
    CHECK( swap_conf_itr != swap_conf_tbl.end(), "swap conf not existing: " + to_string(swap_amount) )
    
    swap_conf_tbl.modify( swap_conf_itr, get_self(), [&]( auto& swap_conf ) {
        swap_conf.mine_token_remained = amount;
    });

}
    
void amax_one::delswapconf( const name& account, const uint64_t amount)
{
    require_auth( account );
    CHECK(account == _self || account == _gstate.admin , "no auth for operate");

    swap_conf_t::tbl_t swap_conf_tbl(get_self(), get_self().value);
    auto swap_conf_itr = swap_conf_tbl.find(amount);
    CHECK( swap_conf_itr != swap_conf_tbl.end(), "swap conf not found: " + to_string(amount) )
    swap_conf_tbl.erase(swap_conf_itr);
}

void amax_one::_claim_reward( const name&   to, 
                        const asset&        recd_apls,
                        bool                ads_checked,
                        const string&       ads_id, 
                        const string&       memo ) 
{
    swap_conf_t::tbl_t swap_conf_tbl(get_self(), get_self().value);
    auto swap_conf_itr = swap_conf_tbl.find(recd_apls.amount);
    CHECK( swap_conf_itr != swap_conf_tbl.end(), "swap conf not found: " + recd_apls.to_string() )

    asset swap_tokens = swap_conf_itr->swap_tokens;
    if (ads_checked) swap_tokens = swap_conf_itr->swap_tokens_after_adscheck;

    CHECK(swap_tokens.amount > 0, "swap token must greater than 0");

    CHECK( swap_tokens <= swap_conf_itr->mine_token_remained, "reward token not enough" )
    swap_conf_tbl.modify( swap_conf_itr, get_self(), [&]( auto& swap_conf ) {
        swap_conf.mine_token_remained = swap_conf_itr->mine_token_remained - swap_tokens;
    });

    TRANSFER(_gstate.mine_token_contract, to, swap_tokens, memo )
    _on_apl_swap_log(to, recd_apls, swap_tokens, ads_id, current_time_point());

}

void amax_one::_on_apl_swap_log(
                    const name&         miner,
                    const asset&        recd_apls,
                    const asset&        swap_tokens,
                    const string&       ads_id,
                    const time_point&   created_at) {	
    amax_one::aplswaplog_action act{ _self, { {_self, active_permission} } };
    act.send( miner, recd_apls, swap_tokens, ads_id, created_at );
}


string_view amax_one::_get_ads_id (const string& memo) {
    if(memo.rfind("adsid:", 0 ) == 0) {
        vector<string_view> params = split(memo, ":");
        if( params.size() == 2)
            return params[1];
        
    }
    return "";
}