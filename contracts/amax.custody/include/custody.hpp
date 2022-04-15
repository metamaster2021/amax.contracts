#include "custodydb.hpp"

using namespace std;
using namespace wasm::db;

static constexpr bool DEBUG = true;

#define WASM_FUNCTION_PRINT_LENGTH 50

#define AMAX_LOG( debug, exception, ... ) {  \
if ( debug ) {                               \
   std::string str = std::string(__FILE__); \
   str += std::string(":");                 \
   str += std::to_string(__LINE__);         \
   str += std::string(":[");                \
   str += std::string(__FUNCTION__);        \
   str += std::string("]");                 \
   while(str.size() <= WASM_FUNCTION_PRINT_LENGTH) str += std::string(" ");\
   eosio::print(str);                                                             \
   eosio::print( __VA_ARGS__ ); }}

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
    [[eosio::action]] void addplan(const name& issuer, const string& title, const name& asset_contract, const symbol& asset_symbol, const uint64_t& unlock_interval_days, const int64_t& unlock_time);
    [[eosio::action]] void setplanowner(const name& issuer, const uint64_t& plan_id, const name& new_owner);
    [[eosio::action]] void enableplan(name issuer, uint16_t plan_id, bool enabled);
    [[eosio::action]] void delplan(name issuer, uint16_t plan_id);
    
    [[eosio::on_notify("*::transfer")]]
    void ontransfer(name from, name to, asset quantity, string memo);

    [[eosio::action]] void redeem(name issuer, name to, uint64_t plan_id);
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