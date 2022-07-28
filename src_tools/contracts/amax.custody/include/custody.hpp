#include "custodydb.hpp"

using namespace std;
using namespace wasm::db;

class [[eosio::contract("amax.custody")]] custody: public eosio::contract {
private:
    global_singleton    _global;
    global_t            _gstate;

public:
    using contract::contract;

    custody(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds),
        _global(get_self(), get_self().value)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ACTION init();
    ACTION fixissue(const uint64_t& issue_id, const asset& issued, const asset& locked, const asset& unlocked);
    ACTION setconfig(const asset &plan_fee, const name &fee_receiver);
    ACTION addplan(const name& owner, const string& title, const name& asset_contract, const symbol& asset_symbol, const uint64_t& unlock_interval_days, const int64_t& unlock_times);
    ACTION setplanowner(const name& owner, const uint64_t& plan_id, const name& new_owner);
    ACTION enableplan(const name& owner, const uint64_t& plan_id, bool enabled);
    ACTION delendissue(const uint64_t& issue_id);
    /**
     * @require by maintainer only
     * The delplan action will affect table scanning
     */
    // [[eosio::action]] void delplan(const name& owner, const uint64_t& plan_id);

    /**
     * ontransfer, trigger by recipient of transfer()
     * @param memo - memo format:
     * 1. plan:${plan_id}, pay plan fee, Eg: "plan:" or "plan:1"
     *    pay plan fee
     *
     * 2. issue:${receiver}:${plan_id}:${first_unlock_days}, Eg: "issue:receiver1234:1:30"
     *
     *    add issue, the owner
     *    @param receiver - owner name
     *    @param plan_id - plan id
     *    @param first_unlock_days - first unlock days after created
     *
     *    transfer() params:
     *    @param from - issuer
     *    @param to   - must be contract self
     *    @param quantity - issued quantity
     */
    [[eosio::on_notify("*::transfer")]] void ontransfer(name from, name to, asset quantity, string memo);
    [[eosio::action]] void unlock(const name& unlocker, const uint64_t& plan_id, const uint64_t& issue_id);
    /**
     * @require run by issuer only
     */
    [[eosio::action]] void endissue(const name& issuer, const uint64_t& plan_id, const uint64_t& issue_id);
private:
    void _unlock(const name& actor, const uint64_t& plan_id,
                         const uint64_t& issue_id, bool is_end_action);
}; //contract custody