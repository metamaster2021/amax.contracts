
#include <amax.token/amax.token.hpp>
#include "custody.hpp"
#include "utils.hpp"

#include <chrono>

using std::chrono::system_clock;
using namespace wasm;

static constexpr eosio::name active_permission{"active"_n};

// transfer out from contract self
#define TRANSFER_OUT(token_contract, to, quantity, memo) token::transfer_action(                                \
                                                             token_contract, {{get_self(), active_permission}}) \
                                                             .send(                                             \
                                                                 get_self(), to, quantity, memo);

[[eosio::action]]
void custody::setconfig(const asset &plan_fee, const name &fee_receiver) {
    require_auth(get_self());
    CHECK(plan_fee.symbol == SYS_SYMBOL, "plan_fee symbol mismatch with sys symbol")
    CHECK(plan_fee.amount >= 0, "plan_fee symbol amount can not be negative")
    CHECK(is_account(fee_receiver), "fee_receiver account does not exist")
    _gstate.plan_fee = plan_fee;
    _gstate.fee_receiver = fee_receiver;
    _global.set( _gstate, get_self() );
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
    if (_gstate.plan_fee.amount > 0) {
        CHECK(_gstate.fee_receiver.value != 0, "fee_receiver not set")
    }
    plan_t::tbl_t plan_tbl(get_self(), get_self().value);
	auto plan_id = plan_tbl.available_primary_key();
    if (plan_id == 0) plan_id = 1;
    plan_tbl.emplace( owner, [&]( auto& plan ) {
        plan.id = plan_id;
        plan.owner = owner;
        plan.title = title;
        plan.asset_contract = asset_contract;
        plan.asset_symbol = asset_symbol;
        plan.unlock_interval_days = unlock_interval_days;
        plan.unlock_times = unlock_times;
        plan.status =  _gstate.plan_fee.amount != 0 ? PLAN_UNPAID_FEE : PLAN_ENABLED;
        plan.created_at = current_time_point();
        plan.updated_at = plan.created_at;
    });
    account::tbl_t account_tbl(get_self(), get_self().value);
    account_tbl.set(owner.value, owner, [&]( auto& acct ) {
            acct.owner = owner;
            acct.last_plan_id = plan_id;
    });
}

[[eosio::action]]
void custody::setplanowner(const name& owner, const uint64_t& plan_id, const name& new_owner){
    plan_t::tbl_t plan_tbl(get_self(), get_self().value);
    auto plan_itr = plan_tbl.find(plan_id);
    CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(plan_id) )
    CHECK( owner == plan_itr->owner, "owner mismatch" )
    CHECK( has_auth(plan_itr->owner) || has_auth(get_self()), "Missing required authority of owner or maintainer" )
    CHECK( is_account(new_owner), "new_owner account does not exist");

    plan_tbl.modify( plan_itr, same_payer, [&]( auto& plan ) {
        plan.owner = new_owner;
        plan.updated_at = current_time_point();
    });
}

// [[eosio::action]]
// void custody::delplan(const name& owner, const uint64_t& plan_id) {
//     require_auth(get_self());

//     plan_t::tbl_t plan_tbl(get_self(), get_self().value);
//     auto plan_itr = plan_tbl.find(plan_id);
//     CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(plan_id) )
//     CHECK( owner == plan_itr->owner, "owner mismatch" )

//     plan_tbl.erase(plan_itr);
// }

[[eosio::action]]
void custody::enableplan(const name& owner, const uint64_t& plan_id, bool enabled) {
    require_auth(owner);

    plan_t::tbl_t plan_tbl(get_self(), get_self().value);
    auto plan_itr = plan_tbl.find(plan_id);
    CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(plan_id) )
    CHECK( owner == plan_itr->owner, "owner mismatch" )
    CHECK( plan_itr->status != PLAN_UNPAID_FEE, "plan is unpaid fee status" )
    plan_status_t new_status = enabled ? PLAN_ENABLED : PLAN_DISABLED;
    CHECK( plan_itr->status != new_status, "plan status is no changed" )

    plan_tbl.modify( plan_itr, same_payer, [&]( auto& plan ) {
        plan.status = new_status;
        plan.updated_at = current_time_point();
    });
}

void custody::addissue( const name& issuer, const name& receiver, uint64_t plan_id,
                        uint64_t first_unlock_days, const asset& quantity)
{
    require_auth( issuer );

    plan_t::tbl_t plan_tbl(get_self(), get_self().value);
    auto plan_itr = plan_tbl.find(plan_id);
    CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(plan_id) )
    CHECK( plan_itr->status == PLAN_ENABLED, "plan not enabled, status:" + to_string(plan_itr->status) )

    CHECK( is_account(receiver), "receiver account not exist" );
    CHECK( first_unlock_days <= MAX_LOCK_DAYS,
        "unlock_days must be > 0 and <= 365*10, i.e. 10 years" )
    CHECK( quantity.symbol == plan_itr->asset_symbol, "symbol of quantity mismatch with symbol of plan" );
	CHECK( quantity.amount > 0, "quantity must be positive" )

    auto now = current_time_point();

    issue_t::tbl_t issue_tbl(get_self(), get_self().value);
    auto issue_id = issue_tbl.available_primary_key();
    if (issue_id == 0) issue_id = 1;

    issue_tbl.emplace( issuer, [&]( auto& issue ) {
        issue.issue_id = issue_id;
        issue.plan_id = plan_id;
        issue.issuer = issuer;
        issue.receiver = receiver;
        issue.first_unlock_days = first_unlock_days;
        issue.issued = quantity.amount;
        issue.locked = 0;
        issue.unlocked = 0;
        issue.status = ISSUE_UNDEPOSITED;
        issue.issued_at = now;
        issue.updated_at = now;
    });

    account::tbl_t account_tbl(get_self(), get_self().value);
    account_tbl.set(issuer.value, issuer, [&]( auto& acct ) {
            acct.owner = issuer;
            acct.last_issue_id = issue_id;
    });
}

//issue-in op: transfer tokens to the contract and lock them according to the given plan
[[eosio::action]]
void custody::ontransfer(name from, name to, asset quantity, string memo) {
    if (from == get_self() || to != get_self()) return;

	CHECK( quantity.amount > 0, "quantity must be positive" )

    //memo params format:
    //plan:${plan_id}, Eg: "plan:" or "plan:1"
    //issue:${issue_id}, Eg: "issue:" or "issue:1"
    vector<string_view> memo_params = split(memo, ":");
    ASSERT(memo_params.size() > 0);
    if (memo_params[0] == "plan") {
        CHECK(memo_params.size() == 2, "ontransfer:plan params size of must be 2")
        auto param_plan_id = memo_params[1];

        CHECK(get_first_receiver() == SYS_BANK, "must transfer by contract: " + SYS_BANK.to_string());
        CHECK( quantity.symbol == SYS_SYMBOL, "quantity symbol mismatch with fee symbol");
        CHECK( quantity.amount == _gstate.plan_fee.amount,
            "quantity amount mismatch with fee amount: " + to_string(_gstate.plan_fee.amount) );
        uint64_t plan_id = 0;
        if (param_plan_id.empty()) {
            account::tbl_t account_tbl(get_self(), get_self().value);
            auto acct = account_tbl.get(from.value, "from account does not exist in custody constract");
            plan_id = acct.last_plan_id;
            CHECK( plan_id != 0, "from account does no have any plan" );
        } else {
            plan_id = to_uint64(param_plan_id.data(), "plan_id");
            CHECK( plan_id != 0, "plan id can not be 0" );
        }

        plan_t::tbl_t plan_tbl(get_self(), get_self().value);
        auto plan_itr = plan_tbl.find(plan_id);
        CHECK( plan_itr != plan_tbl.end(), "plan not found by plan_id: " + to_string(plan_id) )
        CHECK( plan_itr->status == PLAN_UNPAID_FEE, "plan must be unpaid fee, status:" + to_string(plan_itr->status) )
        plan_tbl.modify( plan_itr, same_payer, [&]( auto& plan ) {
            plan.status = PLAN_ENABLED;
            plan.updated_at = current_time_point();
        });

        TRANSFER_OUT( get_first_receiver(), _gstate.fee_receiver, quantity, memo )
    } else if (memo_params[0] == "issue") {
        CHECK(memo_params.size() == 2, "ontransfer:issue params size of must be 2")
        auto param_issue_id = memo_params[1];
        uint64_t issue_id = 0;
        if (param_issue_id.empty()) {
            account::tbl_t account_tbl(get_self(), get_self().value);
            auto acct = account_tbl.get(from.value, "from account does not exist in custody constract");
            issue_id = acct.last_issue_id;
            CHECK( issue_id != 0, "from account does no have any issue" );
        } else {
            issue_id = std::strtoul(param_issue_id.data(), nullptr, 10);
            CHECK( issue_id != 0, "issue id can not be 0" );
        }

        issue_t::tbl_t issue_tbl(get_self(), get_self().value);
        auto issue_itr = issue_tbl.find(issue_id);
        CHECK( issue_itr != issue_tbl.end(), "issue not found: " + to_string(issue_id) )
        CHECK( issue_itr->status == ISSUE_UNDEPOSITED, "issue must be undeposited, status: " + to_string(issue_itr->status) );

        plan_t::tbl_t plan_tbl(get_self(), get_self().value);
        auto plan_itr = plan_tbl.find(issue_itr->plan_id);
        CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(issue_itr->plan_id) )
        CHECK( plan_itr->status == PLAN_ENABLED, "plan not enabled, status:" + to_string(plan_itr->status) )

        auto asset_contract = get_first_receiver();
        CHECK( plan_itr->asset_contract == asset_contract, "issue asset contract mismatch" );
        CHECK( plan_itr->asset_symbol == quantity.symbol, "issue asset symbol mismatch" );

        CHECK( issue_itr->issued == quantity.amount, "issue amount mismatch" );

        auto now = current_time_point();
        plan_tbl.modify( plan_itr, same_payer, [&]( auto& plan ) {
            plan.total_issued += quantity.amount;
            plan.updated_at = now;
        });

        issue_tbl.modify( issue_itr, same_payer, [&]( auto& issue ) {
            issue.locked = quantity.amount;
            issue.status = ISSUE_NORMAL;
            issue.updated_at = now;
        });
    }
    // else { ignore }
}

[[eosio::action]]
void custody::endissue(const name& issuer, const uint64_t& plan_id, const uint64_t& issue_id) {
    require_auth( issuer );

    internal_unlock(issuer, plan_id, issue_id, /*is_end_action=*/true);
}

/**
 * withraw all available/unlocked assets belonging to the issuer
 */
[[eosio::action]]
void custody::unlock(const name& receiver, const uint64_t& plan_id, const uint64_t& issue_id) {
    require_auth(receiver);

    internal_unlock(receiver, plan_id, issue_id, /*is_end_action=*/false);
}

void custody::internal_unlock(const name& actor, const uint64_t& plan_id,
    const uint64_t& issue_id, bool is_end_action)
{
    auto now = current_time_point();

    issue_t::tbl_t issue_tbl(get_self(), get_self().value);
    auto issue_itr = issue_tbl.find(issue_id);
    CHECK( issue_itr != issue_tbl.end(), "issue not found: " + to_string(issue_id) )

    if (is_end_action)  {
        CHECK( issue_itr->issuer == actor, "issuer mismatch" )
    } else {
        CHECK( issue_itr->receiver == actor, "receiver mismatch" )
    }

    CHECK( issue_itr->plan_id == plan_id, "plan id mismatch" )
    plan_t::tbl_t plan_tbl(get_self(), get_self().value);
    auto plan_itr = plan_tbl.find(plan_id);
    CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(plan_id) )
    CHECK( plan_itr->status == PLAN_ENABLED, "plan not enabled, status:" + to_string(plan_itr->status) )

    if (is_end_action) {
        CHECK( issue_itr->status != ISSUE_ENDED,
            "issue has been ended, status: " + to_string(issue_itr->status) );
    } else {
        CHECK( issue_itr->status == ISSUE_NORMAL,
            "issue not normal, status: " + to_string(issue_itr->status) );
    }

    uint64_t total_unlocked = 0;
    uint64_t remaining_locked = issue_itr->locked;
    if (issue_itr->status == ISSUE_NORMAL) {
        ASSERT(now >= issue_itr->issued_at)
        auto issued_days = (now.sec_since_epoch() - issue_itr->issued_at.sec_since_epoch()) / DAY_SECONDS;
        auto unlocked_days = issued_days > issue_itr->first_unlock_days ? issued_days - issue_itr->first_unlock_days : 0;
        ASSERT(plan_itr->unlock_interval_days > 0);
        auto unlocked_times = std::min(unlocked_days / plan_itr->unlock_interval_days, plan_itr->unlock_times);
        if (unlocked_times >= plan_itr->unlock_times) {
            total_unlocked = issue_itr->issued;
        } else {
            ASSERT(plan_itr->unlock_times > 0)
            total_unlocked = multiply_decimal64(issue_itr->issued, unlocked_times, plan_itr->unlock_times);
            ASSERT(total_unlocked >= issue_itr->unlocked && issue_itr->issued >= total_unlocked)
        }

        auto cur_unlocked = total_unlocked - issue_itr->unlocked;
        remaining_locked = issue_itr->issued - total_unlocked;

        TRACE("unlock detail: ", PP0(issued_days), PP(unlocked_days), PP(unlocked_times), PP(total_unlocked),
            PP(cur_unlocked), PP(remaining_locked), "\n");

        if (cur_unlocked > 0) {
            auto unlock_quantity = asset(cur_unlocked, plan_itr->asset_symbol);
            string memo = "unlock: " + to_string(issue_id) + "@" + to_string(plan_id);
            TRANSFER_OUT( plan_itr->asset_contract, issue_itr->receiver, unlock_quantity, memo )
        } else { // cur_unlocked == 0
            if (!is_end_action) {
                CHECK( false, "It's not time to unlock asset yet" )
            } // else ignore
        }

        uint64_t refunded = 0;
        if (is_end_action && remaining_locked > 0) {
            refunded = remaining_locked;
            auto memo = "refund: " + to_string(issue_id);
            auto refunded_quantity = asset(refunded, plan_itr->asset_symbol);
            TRANSFER_OUT( plan_itr->asset_contract, issue_itr->issuer, refunded_quantity, memo )
            remaining_locked = 0;
        }
        plan_tbl.modify( plan_itr, same_payer, [&]( auto& plan ) {
            plan.total_unlocked += cur_unlocked;
            if (refunded > 0) {
                plan.total_refunded += refunded;
            }
            plan.updated_at = current_time_point();
        });
    }

    issue_tbl.modify( issue_itr, same_payer, [&]( auto& issue ) {
        issue.unlocked = total_unlocked;
        issue.locked = remaining_locked;
        if (is_end_action || total_unlocked == issue.issued) {
            issue.status = ISSUE_ENDED;
        }
        issue.updated_at = current_time_point();
    });
}