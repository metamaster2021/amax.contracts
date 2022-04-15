#include "custodydb.hpp"

using namespace std;
using namespace wasm::db;

class [[eosio::contract("amax.custody")]] custody: public eosio::contract {
private:
    dbc                 _db;
    global_singleton    _global;
    global_t            _gstate;

public:
    using contract::contract;

    custody(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds), 
        _global(get_self(), get_self().value), 
        _db(get_self())
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~custody() {
        _global.set( _gstate, get_self() );
    }

    [[eosio::action]] void init(const name& issuer);  //initialize & maintain
    // [[eosio::action]] void addadmin(name issuer, name admin, bool is_supper_admin);
    // [[eosio::action]] void deladmin(name issuer, name admin);
    [[eosio::action]] void addplan(name issuer, string plan_name, name asset_contract, symbol asset_symbol, int64_t unlock_interval_days, int64_t unlock_times);
    [[eosio::action]] void delplan(name issuer, uint16_t plan_id);
    // ACTION propose(name issuer, uint16_t plan_id, uint16_t advance_unlock_days, uint16_t advance_unlock_ratio);
    // ACTION approve(name issuer, checksum256 proposal_txid);
    
    [[eosio::on_notify("*::transfer")]]
    void ontransfer(name from, name to, asset quantity, string memo);

    [[eosio::action]] void redeem(name issuer, name to);
    // ACTION withdrawx(name issuer, name to, name original_recipient, uint64_t stake_id, asset quantity);
    // ACTION repairstake(name issuer, name recipient, uint64_t stake_id, uint64_t amount);
    // ACTION repairindex(name issuer, name recipient, uint64_t stake_id);
    // ACTION repairplan(name issuer, name user, uint64_t stake_id, uint16_t plan_id);

private:
    uint64_t _gen_new_id(const name &counter_key);
    uint16_t _get_accumulated_ratio(plan_t &t, time_point& staked_at);
    uint64_t _get_admin_counter();
    uint64_t _inc_admin_counter(bool increase_by_one = true);

}; //contract custody