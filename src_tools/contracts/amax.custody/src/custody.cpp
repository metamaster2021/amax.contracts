
#include <amax.token.hpp>
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
void custody::init() {
    require_auth( _self );

    // auto issues = issue_t::tbl_t(_self, _self.value);
    // auto itr = issues.begin();
    // int step = 0;
    // while (itr != issues.end()) {
    //     if (step > 30) return;

    //     if (itr->plan_id != 1 || itr->issuer != "armoniaadmin"_n) {
    //         itr = issues.erase( itr );
    //         step++;
    //     } else
    //         itr++;
    // }

    auto plans = plan_t::tbl_t(_self, _self.value);
    auto itr = plans.begin();
    int step = 0;
    while (itr != plans.end()) {
        if (step > 30) return;

        if (itr->id != 1) {
            itr = plans.erase( itr );
            step++;
        } else
            itr++;
    }

    check( step > 0, "none deleted" );
}

[[eosio::action]]
void custody::fixissue(const uint64_t& issue_id, const asset& issued, const asset& locked, const asset& unlocked) {
    require_auth(get_self());

    issue_t::tbl_t issue_tbl(get_self(), get_self().value);
    auto itr = issue_tbl.find(issue_id);
    check( itr != issue_tbl.end(), "issue not found: " + to_string(issue_id) );
    check( issued.symbol == itr->issued.symbol, "issued symbol mismatch");
    check( locked.symbol == itr->locked.symbol, "locked symbol mismatch");
    check( unlocked.symbol == itr->unlocked.symbol, "unlocked symbol mismatch");

    check( issued.amount >= 0, "issued amount can not be negtive");
    check( locked.amount >= 0, "locked amount can not be negtive");
    check( unlocked.amount >= 0, "unlocked amount can not be negtive");
    check( issued.amount == locked.amount + unlocked.amount,
            "issued.amount must be equal to (locked.amount + unlocked.amount)");

    issue_tbl.modify(itr, get_self(), [&]( auto& issue ) {
        issue.issued = issued;
        issue.locked = locked;
        issue.unlocked = unlocked;
    });
}

[[eosio::action]]
void custody::fixissuedays() {
    require_auth(get_self());
    issue_t::tbl_t issue_tbl(get_self(), get_self().value);
    for (auto itr = issue_tbl.begin(); itr != issue_tbl.end(); itr++) {
        if (itr->first_unlock_days == 0) {
            issue_tbl.modify(itr, same_payer, [&]( auto& issue ) {
                issue.first_unlock_days = issue.unlock_interval_days;
            });
        }
    }
}

void custody::setreceiver(const uint64_t& issue_id, const name& receiver) {
    require_auth(get_self());
    check(is_account(receiver), "receiver account not existed");

    issue_t::tbl_t issue_tbl(get_self(), get_self().value);
    auto itr = issue_tbl.find(issue_id);
    check( itr != issue_tbl.end(), "issue not found: " + to_string(issue_id) );
    issue_tbl.modify(itr, get_self(), [&]( auto& issue ) {
        issue.receiver = receiver;
    });
}

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
        plan.total_issued = asset(0, asset_symbol);
        plan.total_unlocked = asset(0, asset_symbol);
        plan.total_refunded = asset(0, asset_symbol);
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

//issue-in op: transfer tokens to the contract and lock them according to the given plan
[[eosio::action]]
void custody::ontransfer(name from, name to, asset quantity, string memo) {
    if (from == get_self() || to != get_self()) return;

	CHECK( quantity.amount > 0, "quantity must be positive" )

    //memo params format:
    //0. $plan_id, Eg: memo: "66"
    //1. plan:${plan_id}, Eg: memo: "plan:" or "plan:1"
    //2. issue:${receiver}:${plan_id}:${first_unlock_days}, Eg: "issue:receiver1234:1:30"
    vector<string_view> memo_params = split(memo, ":");
    ASSERT(memo_params.size() > 0);
    if( memo_params.size() == 1 ) {

    }
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
        CHECK(memo_params.size() == 4, "ontransfer:issue params size of must be 4")
        auto receiver = name(memo_params[1]);
        auto plan_id = to_uint64(memo_params[2], "plan_id");
        auto first_unlock_days = to_uint64(memo_params[3], "first_unlock_days");

        plan_t::tbl_t plan_tbl(get_self(), get_self().value);
        auto plan_itr = plan_tbl.find(plan_id);
        CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(plan_id) )
        CHECK( plan_itr->status == PLAN_ENABLED, "plan not enabled, status:" + to_string(plan_itr->status) )

        CHECK( is_account(receiver), "receiver account not exist" );
        CHECK( first_unlock_days <= MAX_LOCK_DAYS,
            "unlock_days must be > 0 and <= 365*10, i.e. 10 years" )
        CHECK( quantity.symbol == plan_itr->asset_symbol, "symbol of quantity mismatch with symbol of plan" );
        CHECK( quantity.amount > 0, "quantity must be positive" )
        CHECK( plan_itr->asset_contract == get_first_receiver(), "issue asset contract mismatch" );
        CHECK( plan_itr->asset_symbol == quantity.symbol, "issue asset symbol mismatch" );

        auto now = current_time_point();

        plan_tbl.modify( plan_itr, same_payer, [&]( auto& plan ) {
            plan.total_issued += quantity;
            plan.updated_at = now;
        });

        issue_t::tbl_t issue_tbl(get_self(), get_self().value);
        auto issue_id = issue_tbl.available_primary_key();
        if (issue_id == 0) issue_id = 1;

        issue_tbl.emplace( _self, [&]( auto& issue ) {
            issue.issue_id = issue_id;
            issue.plan_id = plan_id;
            issue.issuer = from;
            issue.receiver = receiver;
            issue.first_unlock_days = first_unlock_days;
            issue.issued = quantity;
            issue.locked = quantity;
            issue.unlocked = asset(0, quantity.symbol);
            issue.unlock_interval_days = plan_itr->unlock_interval_days;
            issue.unlock_times = plan_itr->unlock_times;
            issue.status = ISSUE_NORMAL;
            issue.issued_at = now;
            issue.updated_at = now;
        });

    }
    // else { ignore }
}

[[eosio::action]]
void custody::endissue(const uint64_t& plan_id, const name& issuer, const uint64_t& issue_id) {
    CHECK(has_auth( _self ), "not authorized to end issue" )
    // require_auth( issuer );

    issue_t::tbl_t issue_tbl(get_self(), get_self().value);
    auto issue_itr = issue_tbl.find(issue_id);
    CHECK( issue_itr != issue_tbl.end(), "issue not found: " + to_string(issue_id) )
    CHECK( issue_itr->plan_id == plan_id, "plan id mismatch" )

    CHECK( issue_itr->status != ISSUE_ENDED, "issue already ended, status: " + to_string(issue_itr->status) )
    CHECK( issuer == issue_itr->issuer || issuer == _self, "not authorized" )

    plan_t::tbl_t plan_tbl(get_self(), get_self().value);
    auto plan_itr = plan_tbl.find(plan_id);
    CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(plan_id) )
    CHECK( plan_itr->status == PLAN_ENABLED, "plan not enabled, status:" + to_string(plan_itr->status) )

    auto to_refund = issue_itr->locked;
    TRANSFER_OUT( plan_itr->asset_contract, issue_itr->issuer, to_refund, string("refund: " + to_string(issue_id)) )

    auto now = current_time_point();

    plan_tbl.modify( plan_itr, same_payer, [&]( auto& plan ) {
        plan.total_refunded     += to_refund;
        plan.updated_at         = now;
    });

    issue_tbl.modify( issue_itr, same_payer, [&]( auto& issue ) {
        issue.status            = ISSUE_ENDED;
        issue.updated_at        = now;
    });
}

/**
 * withraw all available/unlocked assets belonging to the issuer
 */
[[eosio::action]]
void custody::unlock(const name& issuer, const uint64_t& plan_id, const uint64_t& issue_id) {
    require_auth(issuer);

     auto now = current_time_point();
    issue_t::tbl_t issue_tbl(get_self(), get_self().value);
    auto issue_itr = issue_tbl.find(issue_id);
    CHECK( issue_itr != issue_tbl.end(), "issue not found: " + to_string(issue_id) )
    CHECK( issue_itr->plan_id == plan_id, "plan id mismatch" )
    ASSERT( now >= issue_itr->issued_at );

    plan_t::tbl_t plan_tbl(get_self(), get_self().value);
    auto plan_itr = plan_tbl.find(plan_id);
    CHECK( plan_itr != plan_tbl.end(), "plan not found: " + to_string(plan_id) )
    CHECK( plan_itr->status == PLAN_ENABLED, "plan not enabled, status:" + to_string(plan_itr->status) )
    ASSERT( plan_itr->unlock_times > 0 && plan_itr->unlock_interval_days > 0 );

    auto issued_days = (now.sec_since_epoch() - issue_itr->issued_at.sec_since_epoch()) / DAY_SECONDS;
    int unlocked_days = issued_days - issue_itr->first_unlock_days;
    int64_t total_unlocked = 0;   // already_unlocked + to_be_unlocked
    int64_t remaining_locked = issue_itr->locked.amount;

    CHECK( issue_itr->status == ISSUE_NORMAL, "issue abnormal, status: " + to_string(issue_itr->status) )
    CHECK( unlocked_days >= 0, "premature to unlock by n days, n = " + to_string( -1 * unlocked_days ) )

    auto unlocked_times = 1 + std::min( unlocked_days / plan_itr->unlock_interval_days, plan_itr->unlock_times );
    if( unlocked_times >= plan_itr->unlock_times ) {
        total_unlocked = issue_itr->issued.amount;

    } else {
        // total_unlocked = issued_amount * unlockable_times / total_unlock_times
        total_unlocked = multiply_decimal64(issue_itr->issued.amount, unlocked_times, plan_itr->unlock_times);
        ASSERT(total_unlocked >= issue_itr->unlocked.amount && issue_itr->issued.amount >= total_unlocked)
    }

    int64_t curr_unlockable = total_unlocked - issue_itr->unlocked.amount;
    remaining_locked = issue_itr->issued.amount - total_unlocked;
    ASSERT(remaining_locked >= 0);

    CHECK( curr_unlockable > 0, "premature to unlock" )
    auto unlock_quantity = asset(curr_unlockable, plan_itr->asset_symbol);
    string memo = "unlock: " + to_string(issue_id) + "@" + to_string(plan_id);
    TRANSFER_OUT( plan_itr->asset_contract, issue_itr->receiver, unlock_quantity, memo )

    plan_tbl.modify( plan_itr, same_payer, [&]( auto& plan ) {
        plan.total_unlocked.amount += curr_unlockable;
        plan.updated_at         = now;
    });

    issue_tbl.modify( issue_itr, same_payer, [&]( auto& issue ) {
        issue.unlocked.amount   = total_unlocked;
        issue.locked.amount     = remaining_locked;
        if( issue.unlocked == issue.issued )
            issue.status        = ISSUE_ENDED;
        issue.updated_at        = now;
    });
}

void custody::delendissues(const vector<uint64_t>& issue_ids) {
    issue_t::tbl_t issue_tbl(get_self(), get_self().value);

    for( auto& issue_id : issue_ids ) {
        auto issue_itr = issue_tbl.find(issue_id);
        CHECK( issue_itr != issue_tbl.end(), "issue not found: " + to_string(issue_id) )
        CHECK( issue_itr->status == issue_status_t::ISSUE_ENDED, "issue not ended" )
        issue_tbl.erase( issue_itr );
    }
}
