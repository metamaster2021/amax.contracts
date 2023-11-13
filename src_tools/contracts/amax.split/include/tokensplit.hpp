#include "tokensplit.db.hpp"
#include <wasm_db.hpp>

namespace amax {

using namespace std;
using namespace wasm::db;

static constexpr uint16_t  PCT_BOOST   = 10000;

class [[eosio::contract("amax.split")]] tokensplit: public eosio::contract {
private:
    global_singleton    _global;
    global_t            _gstate;
    global2_singleton    _global2;
    global2_t            _gstate2;
    dbc                 _db;

public:
    using contract::contract;

    tokensplit(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds),
        _global(get_self(), get_self().value),
        _global2(get_self(), get_self().value),
        _db(_self)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
        _gstate2 = _global2.exists() ? _global2.get() : global2_t{};
    }

    ~tokensplit() { 
        _global.set( _gstate, get_self() ); 
        _global2.set( _gstate2, get_self() ); 
    }

public:
    ACTION init( const name& admin,const asset& fee, const uint64_t& min_count, const uint64_t& max_count);

    ACTION paused(const bool& paused);

    ACTION addplan(const name& owner, const string& title, const vector<split_unit_s>& conf, const bool& is_auto);

    ACTION editplan(const name& owner,const uint64_t& plan_id,const vector<split_unit_s>& conf, const bool& is_auto);

    ACTION closeplan(const name& creator, const uint64_t& plan_id);

    ACTION claim( const uint64_t& plan_id, const name& owner);

    ACTION claimall( const uint64_t& plan_id, const name& creator);

    ACTION plantrace( const plan_trace_t& trace);

    [[eosio::on_notify("*::transfer")]]
    void ontransfer(const name& from, const name& to, const asset& quant, const string& memo);

    using plantrace_action = eosio::action_wrapper<"plantrace"_n, &tokensplit::plantrace>; 
    using claimall_action = eosio::action_wrapper<"claimall"_n, &tokensplit::claimall>; 

private:
    void _recharge( const name& owner, const asset& quantity);
    void _split( const name& from, const uint64_t& plan_id, const asset& quantity,const uint64_t& boost);
    void _add_wallet( const name& owner, const uint64_t& plan_id, const name& contract, const asset& quantity);
    bool _empty_wallets(const uint64_t& plan_id);

}; //contract split

} //namespace amax