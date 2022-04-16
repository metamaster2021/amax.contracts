
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

    plan.total_issued_amount += quantity.amount;
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
void custody::endplan(const name& issuer, const uint64_t& plan_id, const name& issue_owner) {
    require_auth( issuer );

    plan_t plan(plan_id);
    CHECK( _db.get(plan), "plan not found: " + to_string(plan_id) )
    CHECK( plan.owner == issuer, "issuer not the plan owner!" )

    issue_t::tbl_t issues(_self, plan_id);
    auto issue_idx = issues.get_index<"ownerissues"_n>();
    auto lower_itr = issue_idx.lower_bound( uint128_t(issue_owner.value) << 64 );
	auto upper_itr = issue_idx.upper_bound( uint128_t(issue_owner.value) << 64 | std::numeric_limits<uint64_t>::max() );

	int step = 0;
    for (auto itr = lower_itr; itr != upper_itr && itr != issue_idx.end(); itr++) {
		if (step++ == _gstate.trx_max_step) break;

        unlock(issuer, itr->plan_id, itr->issue_id);
        issue_t issue(itr->plan_id, itr->issue_id);
        _db.get(issue);
        
        auto memo = "terminated: " + to_string(itr->issue_id);
        auto quantity = asset(itr->issued, plan.asset_symbol);
        TRANSFER( plan.asset_contract, issuer, quantity, memo )

        _db.del(issue);
    }
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
    TRANSFER( plan.asset_contract, issue.owner, quantity, memo )
    issue.unlocked = total_unlock;
    issue.locked = issue.issued - total_unlock;

    _db.set(issue);
}