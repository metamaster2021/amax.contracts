
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

    // CHECK(false, "init already done!");
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
    CHECK(_db.get(plan), "plan not exist");
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

void custody::addissue(const name& issuer, const name& receiver, uint64_t plan_id, uint64_t first_unlock_days) {
    require_auth( issuer );

    plan_t plan(plan_id);
    CHECK( _db.get(plan), "plan not found: " + to_string(plan_id) )
    CHECK( plan.enabled, "plan not enabled" )

    _db.set(plan);

    CHECK( is_account(receiver), "receiver account not exist" );

    auto now = current_time_point();

    issue_t::tbl_t issue_tbl(get_self(), get_self().value);
    auto issue_id = issue_tbl.available_primary_key();
    if (issue_id == 0)
        issue_id = 1;
    issue_tbl.emplace( issuer, [&]( auto& issue ) {
        issue.plan_id = plan.id;
        issue.issue_id = issue_id;
        issue.issuer = issuer;
        issue.receiver = receiver;
        issue.first_unlock_days = first_unlock_days;
        issue.status = ISSUE_UNACTIVATED;
        issue.issued_at = now;
        issue.updated_at = now;
    });
}

//issue-in op: transfer tokens to the contract and lock them according to the given plan
[[eosio::action]]
void custody::ontransfer(name from, name to, asset quantity, string memo) {
    if (from == get_self() || to != get_self()) return;

	// CHECK( quantity.symbol.is_valid(), "Invalid quantity symbol name" )
	// CHECK( quantity.is_valid(), "Invalid quantity")
	CHECK( quantity.amount > 0, "quantity must be positive" )

    //memo: issue:${id}, Eg: "issue:" or "issue:1"
    vector<string_view> memo_params = split(memo, ":");
    if (memo_params.size() == 2 && memo_params[0] == "issue") {
        uint64_t issue_id = 0;
        if (!memo_params[0].empty()) {
            issue_id = std::strtoul(memo_params[0].data(), nullptr, 10);
        }

        CHECK( issue_id != 0, "issue id can not be 0" );

        issue_t::tbl_t issue_tbl(get_self(), get_self().value);
        auto issue_itr = issue_tbl.find(issue_id);
        CHECK( issue_itr != issue_tbl.end(), "issue not found: " + to_string(issue_id) )

        plan_t::tbl_t plan_tbl(get_self(), get_self().value);
        auto plan_itr = plan_tbl.find(issue_itr->plan_id);
        CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(issue_itr->plan_id) )
        CHECK( plan_itr->enabled, "plan not enabled" )

        auto asset_contract = get_first_receiver();
        CHECK( plan_itr->asset_contract == asset_contract, "issue asset contract mismatch" );
        CHECK( plan_itr->asset_symbol == quantity.symbol, "issue asset symbol mismatch" );

        CHECK( issue_itr->status == ISSUE_UNACTIVATED, "issue can not be activated at status: " + to_string(issue_itr->status) );
        CHECK( issue_itr->issued == quantity.amount, "issue amount mismatch" );

        plan_tbl.modify( plan_itr, same_payer, [&]( auto& plan ) {
            plan.total_issued += quantity.amount;
        });

        issue_tbl.modify( issue_itr, same_payer, [&]( auto& issue ) {
            issue.status = ISSUE_ACTIVATED;
        });
    }
    // else { ignore }
}

[[eosio::action]]
void custody::endissue(const name& issuer, const uint64_t& plan_id, const uint64_t& issue_id) {
    require_auth( issuer );

    issue_t::tbl_t issue_tbl(get_self(), get_self().value);
    auto issue_itr = issue_tbl.find(issue_id);
    CHECK( issue_itr != issue_tbl.end(), "issue not found: " + to_string(issue_id) )

    CHECK( issue_itr->issuer == issuer, "issuer mismatch" )
    CHECK( issue_itr->plan_id == plan_id, "plan id mismatch" )

    plan_t::tbl_t plan_tbl(get_self(), get_self().value);
    auto plan_itr = plan_tbl.find(issue_itr->plan_id);
    CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(issue_itr->plan_id) )
    CHECK( plan_itr->enabled, "plan not enabled" )
    CHECK( issue_itr->status == ISSUE_UNACTIVATED || issue_itr->status == ISSUE_ACTIVATED,
        "issue has been ended, status: " + to_string(issue_itr->status) );

    if (issue_itr->status == ISSUE_ACTIVATED) {
        // TODO: internal_unlock(), should not check the unlock values
        unlock(issue_itr->receiver, plan_id, issue_id);

        auto memo = "refund: " + to_string(issue_id);
        auto refund = asset(issue_itr->locked, plan_itr->asset_symbol);
        TRANSFER_OUT( plan_itr->asset_contract, issuer, refund, memo )
    }

    // TODO: update plan.total_refund

    issue_tbl.erase(issue_itr); // TODO: can not erase, because app should scan issue table's all updates
    issue_tbl.modify( issue_itr, same_payer, [&]( auto& issue ) {
        issue.status = ISSUE_ENDED;
    });
}

/**
 * withraw all available/unlocked assets belonging to the issuer
 */
[[eosio::action]]
void custody::unlock(const name& receiver, const uint64_t& plan_id, const uint64_t& issue_id) {
    require_auth(receiver);

    auto now = current_time_point();

    issue_t::tbl_t issue_tbl(get_self(), get_self().value);
    auto issue_itr = issue_tbl.find(issue_id);
    CHECK( issue_itr != issue_tbl.end(), "issue not found: " + to_string(issue_id) )

    CHECK( issue_itr->receiver == receiver, "issuer mismatch" )
    CHECK( issue_itr->plan_id == plan_id, "plan id mismatch" )

    plan_t::tbl_t plan_tbl(get_self(), get_self().value);
    auto plan_itr = plan_tbl.find(plan_id);
    CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(plan_id) )
    CHECK( plan_itr->enabled, "plan not enabled" )

    ASSERT(now >= issue_itr->issued_at)
    auto issued_days = (now.sec_since_epoch() - issue_itr->issued_at.sec_since_epoch()) / DAY_SECONDS;
    auto unlocked_days = issued_days > issue_itr->first_unlock_days ? issued_days - issue_itr->first_unlock_days : 0;
    ASSERT(plan_itr->unlock_interval_days > 0);
    auto unlocked_times = std::min(unlocked_days / plan_itr->unlock_interval_days, plan_itr->unlock_times);
    auto unlocked_per_times = issue_itr->issued / plan_itr->unlock_times;

    ASSERT(plan_itr->unlock_times > 0)
    auto total_unlocked = multiply_decimal64(issue_itr->issued, unlocked_times, plan_itr->unlock_times);
    ASSERT(total_unlocked >= issue_itr->unlocked && issue_itr->issued > total_unlocked)
    auto cur_unlocked = total_unlocked - issue_itr->unlocked;
    auto remaining_locked = issue_itr->issued - total_unlocked;

    CHECK( cur_unlocked > 0, "already unlocked" )
    auto quantity = asset(cur_unlocked, plan_itr->asset_symbol);
    string memo = "unlock: " + to_string(issue_id) + "@" + to_string(plan_id);
    TRANSFER_OUT( plan_itr->asset_contract, issue_itr->receiver, quantity, memo )

    // TODO: update plan.total_unlock
    issue_tbl.modify( issue_itr, same_payer, [&]( auto& issue ) {
        issue.unlocked = total_unlocked;
        issue.locked = remaining_locked;
    });
}