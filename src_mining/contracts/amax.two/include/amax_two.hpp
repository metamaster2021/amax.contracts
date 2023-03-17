#include "amax_two.db.hpp"
#include <eosio/action.hpp>
#include <wasm_db.hpp>


using namespace std;
using namespace wasm::db;

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


    /**
     * ontransfer, trigger by recipient of transfer()
     * @param memo - memo format:
     * 1. ads_id:${ads_id}, pay plan fee, Eg: "ads_id:" or "ads_id:1"
     *
     *    transfer() params:
     *    @param from - issuer
     *    @param to   - must be contract self
     *    @param quantity - issued quantity
     */
    [[eosio::on_notify("aplink.token::transfer")]] void ontransfer(name from, name to, asset quantity, string memo);

    [[eosio::action]] void aplswaplog(
                    const name&         miner,
                    const asset&        recd_apls,
                    const asset&        swap_tokens,
                    const time_point&   created_at);
                    
    [[eosio::action]] void setswapconf(const name& account, const asset& mine_token_total, const asset& mine_token_remained);
     
    using aplswaplog_action = eosio::action_wrapper<"aplswaplog"_n, &amax_two::aplswaplog>;

private: 
    void _claim_reward( const name& to, const asset& recd_apls, const string& memo );
    void _cal_reward( asset& reward, const name& to, const asset& recd_apls );
    void _on_apl_swap_log(
                    const name&         miner,
                    const asset&        recd_apls,
                    const asset&        swap_tokens,
                    const time_point&   created_at);


    
}; //contract amax.two