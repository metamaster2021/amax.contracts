#include "custody.hpp"
#include "utils.hpp"

#include <chrono>


#define div(a, b) divide_decimal(a, b, PRECISION_1)
#define mul(a, b) multiply_decimal(a, b, PRECISION_1)

#define high_div(a, b) divide_decimal(a, b, HIGH_PRECISION_1)
#define high_mul(a,b ) multiply_decimal(a, b, HIGH_PRECISION_1)

using std::chrono::system_clock;

using namespace wasm;


[[eosio::action]] 
void custody::init(const name& issuer) {
    check(get_first_receiver() == _self, "wrong receiver");
    _check_admin_auth(issuer, true);

    check(false, "init not allowed!");

}


[[eosio::action]] 
void custody::addadmin(name issuer, name admin, bool is_supper_admin) {
    check(get_first_receiver() == _self, "wrong receiver");
    check( is_account( admin ), "admin account does not exist" );
    _check_admin_auth(issuer, true);

    admin_t the_admin(admin, is_supper_admin);
    check( !_db.get(the_admin), "admin already exists" );
    _db.set(the_admin);

    _inc_admin_counter();
}

[[eosio::action]]
void custody::deladmin(name issuer, name admin) {
    check( get_first_receiver() == _self, "wrong receiver" );
    _check_admin_auth(issuer, true);

    admin_t the_admin(admin);
    check( _db.get(the_admin), "admin not exist" );

    _db.del(the_admin);

    _inc_admin_counter(false);
}

//add a lock plan
[[eosio::action]] 
void custody::addplan(name issuer, string plan_name, name asset_bank, symbol asset_symbol, int16_t unlock_days) {
    check(get_first_receiver() == _self, "wrong receiver");
    _check_admin_auth(issuer, true);
    check( unlock_days <= MAX_LOCK_DAYS, "unlock_days must be <= 365*10, i.e. 10 years" );

    uint16_t plan_id = _gen_new_id(COUNTER_PLAN);
    plan_t plan(plan_id, plan_name, asset_bank, asset_symbol, unlock_days);
    _db.set(plan);
}

[[eosio::action]] 
void custody::delplan(name issuer, uint16_t plan_id) {
    check(get_first_receiver() == _self, "wrong receiver");
    _check_admin_auth(issuer, true);

    plan_t plan(plan_id);
    check(_db.get(plan), "plan not exist");
    _db.del(plan);

}

/** Propose to add advanced unlock days & ratio **/
// [[eosio::action]] custody::propose(name issuer, uint16_t plan_id, uint16_t advance_unlock_days, uint16_t advance_unlock_ratio) {
//     check(get_first_receiver() == _self, "wrong receiver");
//     require_auth(issuer);

//     check( advance_unlock_days  <= MAX_LOCK_DAYS, "lock_days must be <= 3650" );
//     check( advance_unlock_ratio <= PERCENT_BOOST, "unlock_ratio must be <= 10000" ); //boosted by 100

//     //check plan exists
//     plan_t plan(plan_id);
//     check(_db.get(plan), "plan not exist");

//     auto approveExpiredAt = current_time_point() + seconds(DAY_SECONDS);
//     proposal_t proposal(plan_id, advance_unlock_days, advance_unlock_ratio, approveExpiredAt);

//     _db.set(proposal);

// }

// /** approve a proposal for advance-unlock a plan **/
// [[eosio::action]] custody::approve(name issuer, checksum256 proposal_txid) {
//     check(get_first_receiver() == _self, "wrong receiver");
//     check_admin_auth(issuer);

//     proposal_t proposal(proposal_txid);
//     check( _db.get(proposal), "proposal not exist" );

//     auto required_approve_count = get_admin_counter();
//     if (required_approve_count > MIN_APPROVE_COUNT)
//         required_approve_count = MIN_APPROVE_COUNT;

//     // check( proposal.approval_admins.count(issuer) == 0, "you already approved it" );
//     check( required_approve_count == 0 || proposal.approval_admins.size() < required_approve_count, "the proposal already passed: "
//         + to_string(proposal.approval_admins.size()) + " vs required: " + to_string(required_approve_count) );
//     check( proposal.approve_expired_at >= current_block_time(), "the proposal already expired" );

//     proposal.approval_admins.insert(issuer);
//     _db.set(proposal);

//     uint16_t appove_cnt = proposal.approval_admins.size() + 1;
//     if (appove_cnt >= required_approve_count) {
//         process_proposal_approve(proposal);
//     }
// }

//stake-in op: transfer tokens to the contract and lock them according to the given plan
[[eosio::action]] 
void custody::ontransfer(name from, name to, asset quantity, string memo) {
    if (to != _self) return;

	check( quantity.symbol.is_valid(), "Invalid quantity symbol name" );
	check( quantity.is_valid(), "Invalid quantity");
	check( quantity.symbol == SYS_SYMBOL, "Token Symbol not allowed" );
	check( quantity.amount > 0, "ontransfer quanity must be positive" );

    check( get_first_receiver() == SYS_BANK, "must transfer by SYS_BANK: " + SYS_BANK.to_string() );


    //memo: {$plan_id}:{$recipient}, Eg: "1:0-1"
    vector<string_view> transfer_memo = split(memo, ":");
    check( transfer_memo.size() == 2, "params error" );

    auto plan_id            = (uint16_t) atoi(transfer_memo[0].data());   
    auto recipient          = name(transfer_memo[1]);
    check( is_account(recipient), "recipient not exist" );

    //check if plan id exists or not
    plan_t plan(plan_id);
    check( _db.get(plan), "plan not exist" );
    check( plan.asset_symbol == quantity.symbol, "stake asset symbol mismatch" );

    auto asset_bank = get_first_receiver();
    check( plan.asset_bank == asset_bank, "asset bank mismatches with plan's" );
    plan.total_staked_amount += quantity.amount;
    _db.set(plan);

    uint64_t stake_id = _gen_new_id(COUNTER_STAKE);
    stake_t stake(recipient, stake_id, plan_id, quantity.amount, current_block_time());
    _db.set(stake);

    // stake_index_t stake_index(recipient);
    // _db.get(stake_index);
    // stake_index.stakes.push_back(stake_id);
    // _db.set(stake_index);

    // wasm:hash256 txid = get_txid();
    // stake_tx_t staketx(txid, stake_id);
    // _db.set(staketx);

}

/**
 * withraw all available/unlocked assets belonging to the issuer
 */
[[eosio::action]] 
void custody::redeem(name issuer, name to) {
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
// /**
//  *  This is provided to reverse some miss-issued airdrops
//  */
// [[eosio::action]] custody::withdrawx(name issuer, name to, name original_recipient, uint64_t stake_id, asset quantity) {
//     check( get_first_receiver() == _self, "wrong receiver" );
//     check( is_account(to), "to is not an account" );
//     check_admin_auth(issuer, true);

//     stake_t stake(original_recipient, stake_id);
//     check( _db.get(stake), "stake (" + to_string(stake_id) + ") not exist " );
//     check( stake.staked_amount >= quantity.amount + stake.redeemed_amount, "redeem amount exceeds" );
//     //check vailid amount.
//     plan_t plan(stake.plan_id);
//     check( _db.get(plan), "plan not exist" );
//     check( plan.asset_symbol == quantity.symbol, "redeem symbol mismatches" );
//     auto asset_bank             = plan.asset_bank;

//     check( stake.staked_amount >= quantity.amount + stake.redeemed_amount, "withraw amount exceeds allowed");

//     plan.total_redeemed_amount  += quantity.amount;
//     _db.set(plan);

//     stake.staked_amount         -= quantity.amount;
//     stake.redeemed_amount       += quantity.amount;
//     stake.updated_at            = current_block_time();
//     _db.set(stake);

//     if (stake.staked_amount == 0) {
//         stake_index_t stake_index(stake.recipient);
//         if (_db.get(stake_index)) {
//             for (auto it = stake_index.stakes.begin(); it != stake_index.stakes.end(); it++) {
//                 if (*it == stake_id) {
//                     stake_index.stakes.erase(it);
//                     break;
//                 }
//             }
//             db::set(stake_index);
//         }
//     }

//     TRANSFER( asset_bank, _self, to, quantity, "withdrawx" )

// }

/**
 * This is to repair stakes data
 */
// [[eosio::action]] custody::repairstake(name issuer, name recipient, uint64_t stake_id, uint64_t amount) {
//     check( get_first_receiver() == _self, "wrong receiver" );
//     check_admin_auth(issuer, true);

//     stake_t stake(recipient, stake_id);
//     check( _db.get(stake), "stake (" + to_string(stake_id) + ") not exist" );

//     stake.staked_amount = amount;
//     _db.set(stake);

//     stake_index_t stake_index(recipient);
//     _db.get(stake_index);
//     stake_index.stakes.push_back(stake_id);
//     _db.set(stake_index);
// }

/**
 * This is to repair stake index for a recipient
//  */
// [[eosio::action]] custody::repairindex(name issuer, name recipient, uint64_t stake_id) {
//     check(get_first_receiver() == _self, "wrong receiver");
//     check_admin_auth(issuer);

//     stake_index_t stake_index(recipient);
//     _db.get(stake_index);
//     stake_index.stakes.push_back(stake_id);
//     _db.set(stake_index);
// }

// [[eosio::action]] custody::repairplan(name issuer, name user, uint64_t stake_id, uint16_t plan_id) {
//     check(get_first_receiver() == _self, "wrong receiver");
//     check_admin_auth(issuer, true);

//     stake_t stake(user, stake_id);
//     check( _db.get(stake), "Err: stake not exist: " + to_string(stake_id) );
//     stake.plan_id = plan_id;
//     _db.set( stake );
// }

/********************* helper functions below ***************************/
void custody::_check_admin_auth(name issuer, bool need_super_admin) {
    auto maintainer = _self;
    admin_t admin(issuer);

    if (issuer != maintainer) { //_maintainer is by default admin
        check( _db.get(admin), "non-admin error" );

        if (need_super_admin)
            check( admin.is_super_admin, "super admin auth error" );

    } else {
        check( maintainer.value != 0, "maintainer disabled error" );
    }

    require_auth(issuer);
}

// void custody::process_proposal_approve(proposal_t &proposal) {
//     plan_t plan(proposal.plan_id);
//     check( _db.get(plan), "plan not exist");

//     unlock_plan_map unlock_plans = plan.advance_unlock_plans;
//     unlock_plans[proposal.advance_unlock_days] =
//         std::make_pair(proposal.advance_unlock_ratio, proposal.id);

//     plan.advance_unlock_plans = unlock_plans;
//     _db.set(plan);

// }

uint16_t custody::_get_accumulated_ratio(plan_t &plan, time_point& staked_at) {
    auto current_time = current_time_point();
    check(current_time >= staked_at, "current time earlier than stake time error!" );
    uint64_t elapse_time = current_time.sec_since_epoch() - staked_at.sec_since_epoch();
    if (elapse_time >= plan.unlock_days * DAY_SECONDS)
        return 10000; //can be fully redeemed (100%)

    uint16_t accumulated_ratio = 0;
    auto advances = plan.advance_unlock_plans;

    for (auto itr = advances.begin(); itr != advances.end(); itr++) {
        if (itr->first * DAY_SECONDS > elapse_time)
            continue;

        accumulated_ratio += std::get<0>(itr->second);
    }
    // check( false, "plan_id= " + to_string(plan.plan_id) + ", advances.size = "
    //     + to_string(advances.size()) + ", accumulated_ratio = " + to_string(accumulated_ratio) );

    if (accumulated_ratio > PERCENT_BOOST)
        accumulated_ratio = PERCENT_BOOST;

    return accumulated_ratio;
}

uint64_t custody::_gen_new_id(const name &counter_key) {
    uint64_t newID = 1;
    counter_t counter(counter_key);
    if (!_db.get(counter)) {
        counter.counter_val = 1;
        _db.set(counter);

        return 1;
    }

    counter.counter_val++;
    _db.set(counter);

    return counter.counter_val;
}

uint64_t custody::_get_admin_counter() {
    counter_t counter(COUNTER_ADMIN);
    if ( !_db.get(counter) ) {
        counter.counter_val = 0;
        _db.set(counter);
    }

    return counter.counter_val;
}

uint64_t custody::_inc_admin_counter(bool increase_by_one) {
    uint64_t newID = 1;
    counter_t counter(COUNTER_ADMIN);
    if (!_db.get(counter)) {
        check( increase_by_one, "only increase allowed");
        counter.counter_val = 1;
        _db.set(counter);

        return 1;
    }

    newID = counter.counter_val + (increase_by_one ? 1 : -1);
    counter.counter_val = newID;
    _db.set(counter);

    return newID;
}