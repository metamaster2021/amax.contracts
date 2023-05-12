#include "amax_two.db.hpp"
#include <eosio/action.hpp>
#include <wasm_db.hpp>


using namespace std;
using namespace wasm::db;

#define CHECKC(exp, code, msg) \
   { if (!(exp)) eosio::check(false, string("[[") + to_string((int)code) + string("]] ") + msg); }

enum class two_err: uint8_t {
   FINISHED               = 0,
   REWARD_NOT_ENOUGH      = 1,
   SBT_NOT_ENOUGH         = 2
};

class [[eosio::contract("amax.two")]] amax_two: public eosio::contract {
private:
    global_singleton    _global;
    global_t            _gstate;

public:
    using contract::contract;

    amax_two(eosio::name receiver, eosio::name code, datastream<const char*> ds):
         contract(receiver, code, ds),
        _global(get_self(), get_self().value)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }
    ~amax_two() { _global.set( _gstate, get_self() ); }


    [[eosio::action]] void init(const name& admin, const name& mine_token_contract, time_point_sec started_at, time_point_sec ended_at);

    [[eosio::action]] void setminetoken(const asset& mine_token_total, const asset& mine_token_remained);


    /**
     * ontransfer, trigger by recipient of transfer()
     *
     *    transfer() params:
     *    @param from - issuer
     *    @param to   - must be contract self
     *    @param quantity - issued quantity
     */
    [[eosio::on_notify("*::transfer")]] void ontransfer(name from, name to, asset quantity, string memo);

    [[eosio::action]] void aplswaplog(
                    const name&         miner,
                    const asset&        recd_apls,
                    const asset&        swap_tokens,
                    const name&         phases,
                    const time_point&   created_at);
                         
    using aplswaplog_action = eosio::action_wrapper<"aplswaplog"_n, &amax_two::aplswaplog>;

private: 
    void _claim_reward( const name& to, const asset& recd_apls, const string& memo );
    void _cal_reward( asset& reward, const name& to, const asset& recd_apls );
    void _on_apl_swap_log(
                    const name&         miner,
                    const asset&        recd_apls,
                    const asset&        swap_tokens,
                    const name&         phases,
                    const time_point&   created_at);


    
}; //contract amax.two