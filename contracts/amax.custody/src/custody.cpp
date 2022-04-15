
#include "amax.token.hpp"
#include "custody.hpp"
#include "utils.hpp"

#include <chrono>

using std::chrono::system_clock;
using namespace wasm;


[[eosio::action]] 
void custody::init(const name& issuer) {
    require_auth(get_self());

    check(false, "init already done!");

}

//add a lock plan
[[eosio::action]] 
void custody::addplan(const name& issuer, 
    const string& title, const name& asset_contract, const symbol& asset_symbol, 
    const uint64_t& unlock_interval_days, const int64_t& unlock_times) {

    require_auth(issuer);
    CHECK( unlock_interval_days <= MAX_LOCK_DAYS, "unlock_days must be <= 365*10, i.e. 10 years" )

    plan_t::tbl_t plans(_self, _self.value);
	auto plan_id = plans.available_primary_key();
    plan_t plan(plan_id, issuer, title, asset_contract, asset_symbol, unlock_interval_days, unlock_times);
    _db.set(plan);
}

[[eosio::action]] 
void custody::setplanowner(const name& issuer, const uint64_t& plan_id, const name& new_owner){
    require_auth( issuer );

    plan_t plan(plan_id);
    CHECK( _db.get(plan), "plan not exist: " + to_string(plan_id) )
    CHECK( plan.owner == issuer || plan.owner != _self, "Non-plan owner nor maintainer not allowed to change owner" )

    plan.owner = new_owner;
    plan.updated_at = current_time_point();

    _db.set( plan );
}

[[eosio::action]] 
void custody::delplan(const name& issuer, const uint64_t& plan_id) {
    require_auth(get_self());

    plan_t plan(plan_id);
    check(_db.get(plan), "plan not exist");
    _db.del(plan);

}

[[eosio::action]] 
void custody::enableplan(const name& issuer, const uint64_t& plan_id, bool enabled) {
    require_auth(issuer);

    plan_t plan(plan_id);
    CHECK( _db.get(plan), "plan not found: " + to_string(plan_id) )
    CHECK( issuer == plan.owner, "issuer not plan owner to enable/disable plan!" )
    CHECK( plan.enabled != enabled, "plan status no changed" )

    plan.enabled = enabled;
    plan.updated_at = current_time_point();
    _db.set(plan);
}

//stake-in op: transfer tokens to the contract and lock them according to the given plan
[[eosio::action]] 
void custody::ontransfer(name from, name to, asset quantity, string memo) {
    if (to != _self) return;

	CHECK( quantity.symbol.is_valid(), "Invalid quantity symbol name" )
	CHECK( quantity.is_valid(), "Invalid quantity")
	CHECK( quantity.amount > 0, "Quantity must be positive" )

    //memo: {$plan_id}:{$owner}, Eg: "1:armonia12345"
    vector<string_view> transfer_memo = split(memo, ":");
    check( transfer_memo.size() == 2, "params error" );

    auto plan_id            = (uint16_t) atoi(transfer_memo[0].data());
    plan_t plan(plan_id);
    CHECK( _db.get(plan), "plan not found: " + to_string(plan_id) )
    CHECK( plan.enabled, "plan not enabled" )

    auto asset_contract = get_first_receiver();
    check( plan.asset_contract == asset_contract, "stake asset contract mismatch" );
    check( plan.asset_symbol == quantity.symbol, "stake asset symbol mismatch" );

    plan.total_staked_amount += quantity.amount;
    _db.set(plan);

    auto owner          = name(transfer_memo[1]);
    check( is_account(owner), "owner not exist" );
    
    stake_t::tbl_t stakes(_self, plan.id);
    auto stake_id = stakes.available_primary_key();
    stake_t stake(plan.id, stake_id, owner, quantity.amount);
    
    _db.set(stake);
}

[[eosio::action]] 
void custody::endplan(const name& issuer, const uint64_t& plan_id, const name& stake_owner) {
    require_auth( issuer );

    plan_t plan(plan_id);
    CHECK( _db.get(plan), "plan not found: " + to_string(plan_id) )
    CHECK( plan.owner == issuer, "issuer not the plan owner!" )

    stake_t::tbl_t stakes(_self, plan_id);
    auto stake_idx = stakes.get_index<"ownerstakes"_n>();
    auto lower_itr = stake_idx.lower_bound( uint128_t(stake_owner.value) << 64 );
	auto upper_itr = stake_idx.upper_bound( uint128_t(stake_owner.value) << 64 | std::numeric_limits<uint64_t>::max() );

	int step = 0;
    for (auto itr = lower_itr; itr != upper_itr && itr != stake_idx.end(); itr++) {
		if (step++ == _gstate.trx_max_step) break;

        redeem(issuer, itr->plan_id, itr->stake_id);
        stake_t stake(itr->plan_id, itr->stake_id);
        _db.get(stake);
        
        TRANSFER( plan.asset_contract, issuer, itr->staked, "terminated: " + to_string(itr->stake_i) )

        _db.del(stake);
    }

}
/**
 * withraw all available/unlocked assets belonging to the issuer
 */
[[eosio::action]] 
void custody::redeem(const name& issuer, const uint64_t& plan_id, const uint64_t& stake_id) {
    require_auth(issuer);

    // stake_index_t stake_index(to);
    // check( _db.get(stake_index), "no stake index for " + to.to_string() );

    auto now = current_time_point();
    bool withdrawn = false;
    // check( false, "stake_index.stakes.size=" + to_string(stake_index.stakes.size()) );

    string plans = "";
    // for (auto it = stake_index.stakes.begin(); it != stake_index.stakes.end(); /* NOTHING */) {
    //     auto& stake_id = *it;
    //     stake_t stake(to, stake_id);
    //     if (!_db.get(stake)) {
    //         it = stake_index.stakes.erase(it);
    //         withdrawn = true;
    //         continue;
    //     }
    //     auto plan_id = stake.plan_id;
    //     plan_t plan(plan_id);
    //     check( _db.get(plan), "plan: " + to_string(plan_id) + " no exist" );

    //     auto ratio          = get_accumulated_ratio(plan, stake.created_at);
    //     auto total_amount   = stake.staked_amount + stake.redeemed_amount;
    //     auto redeemable     = div(mul(total_amount, ratio), PERCENT_BOOST);

    //     // plans += to_string(plan_id) + ": ratio="+ to_string(ratio)
    //     //             + ", total="+ to_string(total_amount)
    //     //             + ", redeemable: " + to_string((uint64_t) redeemable)
    //     //             + ", redeemed: " + to_string(stake.redeemed_amount) + "\n";

    //     if (redeemable == 0 || stake.redeemed_amount == redeemable) {
    //         it++;
    //         continue;
    //     }

    //     auto redeem_amount  = redeemable - stake.redeemed_amount;

    //     plan.total_redeemed_amount  += redeem_amount;
    //     _db.set(plan);

    //     stake.staked_amount         -= redeem_amount;
    //     stake.redeemed_amount       += redeem_amount;
    //     stake.updated_at            = now;
    //     _db.set(stake);

    //     if (stake.staked_amount == 0) {
    //         it = stake_index.stakes.erase(it);
    //     } else
    //         it++;

    //     auto asset_bank             = plan.asset_bank;
    //     auto quantity               = TO_ASSET(redeem_amount, plan.asset_symbol.code());

    //     TRANSFER( asset_bank, _self, to, quantity, "withdraw" )

    //     withdrawn = true;
    // }

    // check(false, plans );

    // if (withdrawn)
    //     // _db.set(stake_index);
    // else
    //     check( false, "none withdrawn" );

}

// uint16_t custody::_get_accumulated_ratio(plan_t &plan, time_point& staked_at) {
//     auto current_time = current_time_point();
//     check(current_time >= staked_at, "current time earlier than stake time error!" );
//     uint64_t elapse_time = current_time.sec_since_epoch() - staked_at.sec_since_epoch();
//     if (elapse_time >= plan.unlock_days * DAY_SECONDS)
//         return 10000; //can be fully redeemed (100%)

//     uint16_t accumulated_ratio = 0;
//     auto advances = plan.advance_unlock_plans;

//     for (auto itr = advances.begin(); itr != advances.end(); itr++) {
//         if (itr->first * DAY_SECONDS > elapse_time)
//             continue;

//         accumulated_ratio += std::get<0>(itr->second);
//     }
//     // check( false, "plan_id= " + to_string(plan.plan_id) + ", advances.size = "
//     //     + to_string(advances.size()) + ", accumulated_ratio = " + to_string(accumulated_ratio) );

//     if (accumulated_ratio > PERCENT_BOOST)
//         accumulated_ratio = PERCENT_BOOST;

//     return accumulated_ratio;
// }