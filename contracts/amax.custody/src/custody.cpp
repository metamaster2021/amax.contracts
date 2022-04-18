
#include <amax.token/amax.token.hpp>
#include "custody.hpp"
#include "utils.hpp"

#include <chrono>

using std::chrono::system_clock;
using namespace wasm;

static constexpr eosio::name active_permission{"active"_n};

// transfer out from contract self
#define TRANSFER_OUT(token_contract, to, quantity, memo) token::transfer_action( \
        token_contract, { {_self, active_permission} } ).send( \
            _self, to, quantity, memo );

[[eosio::action]]
void custody::init() {
    require_auth(get_self());

    // check(false, "init already done!");
    // addplan(issuer, "Community Partner Incentive Plan", "amax.token"_n, SYS_SYMBOL, 91, 10);
    // addplan(issuer, "Developer Incentive Plan", "amax.token"_n, SYS_SYMBOL, 91, 16);

}

//add a lock plan
[[eosio::action]] void custody::addplan(const name& owner,
                                        const string& title, const name& asset_contract, const symbol& asset_symbol,
                                        const uint64_t& unlock_interval_days, const int64_t& unlock_times)
{
    require_auth(owner);
    CHECK( title.size() <= MAX_TITLE_SIZE, "title size must be <= " + to_string(MAX_TITLE_SIZE) )
    CHECK( is_account(asset_contract), "asset contract account does not exist" )
    CHECK( asset_symbol.is_valid(), "Invalid asset symbol" )
    CHECK( unlock_interval_days > 0 && unlock_interval_days <= MAX_LOCK_DAYS,
        "unlock_days must be > 0 and <= 365*10, i.e. 10 years" )
    CHECK( unlock_times > 0, "unlock times must be > 0" )


    plan_t::tbl_t plans(_self, _self.value);
	auto plan_id = plans.available_primary_key();
    plan_t plan(plan_id, owner, title, asset_contract, asset_symbol, unlock_interval_days, unlock_times);
    _db.set(plan);
}

[[eosio::action]]
void custody::setplanowner(const name& owner, const uint64_t& plan_id, const name& new_owner){
    plan_t plan(plan_id);
    CHECK( _db.get(plan), "plan not exist: " + to_string(plan_id) )
    CHECK( plan.owner == owner, "owner mismatch" )
    CHECK( has_auth(plan.owner) || has_auth(get_self()), "Missing required authority of owner or maintainer" )

    plan.owner = new_owner;
    plan.updated_at = current_time_point();

    _db.set( plan );
}

[[eosio::action]]
void custody::delplan(const name& owner, const uint64_t& plan_id) {
    require_auth(get_self());

    plan_t plan(plan_id);
    check(_db.get(plan), "plan not exist");
    CHECK( plan.owner == owner, "owner mismatch" )
    _db.del(plan);

}

[[eosio::action]]
void custody::enableplan(const name& owner, const uint64_t& plan_id, bool enabled) {
    require_auth(owner);

    plan_t plan(plan_id);
    CHECK( _db.get(plan), "plan not found: " + to_string(plan_id) )
    CHECK( owner == plan.owner, "owner mismatch" )
    CHECK( plan.enabled != enabled, "plan status no changed" )

    plan.enabled = enabled;
    plan.updated_at = current_time_point();
    _db.set(plan);
}

//issue-in op: transfer tokens to the contract and lock them according to the given plan
[[eosio::action]]
void custody::ontransfer(name from, name to, asset quantity, string memo) {
    if (to != _self) return;

	CHECK( quantity.symbol.is_valid(), "Invalid quantity symbol name" )
	CHECK( quantity.is_valid(), "Invalid quantity")
	CHECK( quantity.amount > 0, "Quantity must be positive" )

    //memo: ${plan_id}:${owner}:${first_unlock_days}, Eg: "1:armonia12345:91"
    vector<string_view> transfer_memo = split(memo, ":");
    check( transfer_memo.size() == 3, "params error" );

    auto plan_id            = (uint64_t) atoi(transfer_memo[0].data());
    plan_t plan(plan_id);
    CHECK( _db.get(plan), "plan not found: " + to_string(plan_id) )
    CHECK( plan.enabled, "plan not enabled" )

    auto asset_contract = get_first_receiver();
    check( plan.asset_contract == asset_contract, "issue asset contract mismatch" );
    check( plan.asset_symbol == quantity.symbol, "issue asset symbol mismatch" );

    plan.total_issued += quantity.amount;
    _db.set(plan);

    auto owner          = name(transfer_memo[1]);
    check( is_account(owner), "owner not exist" );

    auto first_unlock_days = (uint64_t) atoi(transfer_memo[2].data());

    issue_t::tbl_t issues(_self, plan.id);
    auto issue_id = issues.available_primary_key();
    issue_t issue(plan.id, issue_id, owner, quantity.amount, first_unlock_days);

    _db.set(issue);
}

[[eosio::action]]
void custody::endissue(const name& issuer, const uint64_t& plan_id, const uint64_t& issue_id) {
    require_auth( issuer );

    plan_t plan(plan_id);
    CHECK( _db.get(plan), "plan not found: " + to_string(plan_id) )
    CHECK( plan.owner == issuer, "issuer not the plan owner!" )

    issue_t issue(plan_id, issue_id);
    CHECK( _db.get(issue), "issue not found: " + to_string(issue_id) + "@" + to_string(plan_id) )
    unlock(issuer, plan_id, issue_id);

    auto memo = "terminated: " + to_string(issue.issue_id);
    auto quantity = asset(issue.locked, plan.asset_symbol);
    TRANSFER_OUT( plan.asset_contract, issuer, quantity, memo )

    _db.del(issue);

    // auto issue_idx = issues.get_index<"ownerissues"_n>();
    // auto lower_itr = issue_idx.lower_bound( uint128_t(issue_owner.value) << 64 );
	// auto upper_itr = issue_idx.upper_bound( uint128_t(issue_owner.value) << 64 | std::numeric_limits<uint64_t>::max() );

	// int step = 0;
    // for (auto itr = lower_itr; itr != upper_itr && itr != issue_idx.end(); itr++) {
	// 	if (step++ == _gstate.trx_max_step) break;
    //     issue_t issue(itr->plan_id, itr->issue_id);
    //     _db.get(issue);
    //     //TODO: update plan
    // }
}

/**
 * withraw all available/unlocked assets belonging to the issuer
 */
[[eosio::action]]
void custody::unlock(const name& issuer, const uint64_t& plan_id, const uint64_t& issue_id) {
    require_auth(issuer);

    auto now = current_time_point();

    plan_t plan(plan_id);
    CHECK( _db.get(plan), "plan not found: " + to_string(plan_id) )

    issue_t issue(plan_id, issue_id);
    CHECK( _db.get(issue), "issue not found: " + to_string(issue_id) + "@" + to_string(plan_id) )
    auto unlock_times = ((now - days(issue.first_unlock_days)).sec_since_epoch() / DAY_SECONDS) / plan.unlock_interval_days;
    auto single_unlock = issue.issued / plan.unlock_times;
    auto total_unlock = single_unlock * unlock_times;

    CHECK( issue.locked > 0 && issue.unlocked < total_unlock, "already unlocked" )
    auto quantity = asset(total_unlock - issue.unlocked, plan.asset_symbol);
    string memo = "unlock: " + to_string(issue_id) + "@" + to_string(plan_id);
    TRANSFER_OUT( plan.asset_contract, issue.owner, quantity, memo )
    issue.unlocked = total_unlock;
    issue.locked = issue.issued - total_unlock;

    _db.set(issue);
}