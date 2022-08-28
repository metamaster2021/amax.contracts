#include "amax_one.db.hpp"
#include <eosio/action.hpp>
#include <wasm_db.hpp>


using namespace std;
using namespace wasm::db;

class [[eosio::contract("amax.one")]] amax_one: public eosio::contract {
private:
    global_singleton    _global;
    global_t            _gstate;

public:
    using contract::contract;

    amax_one(eosio::name receiver, eosio::name code, datastream<const char*> ds):
         contract(receiver, code, ds),
        _global(get_self(), get_self().value)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }
    ~amax_one() { _global.set( _gstate, get_self() ); }


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


    [[eosio::action]] void confirmads( const uint64_t& order_id );
    [[eosio::action]] void onswapexpird( const uint64_t& order_id );

     [[eosio::action]] void aplswaplog(
                    const name&         miner,
                    const asset&        recd_apls,
                    const asset&        swap_tokens,
                    const string&       ads_id,
                    const time_point&   created_at);

    [[eosio::action]] void addswapconf(
            const name&         account,
            const uint64_t&     amount,
            const asset&        swap_tokens,
            const asset&        swap_tokens_after_adscheck,
            const asset&        total_amount,
            const asset&        remain_amount);
    
    [[eosio::action]] void delswapconf( const name& account, const uint64_t amount);

    [[eosio::action]] void setremained( const uint64_t& swap_conf_id, const asset& amount);

    using aplswaplog_action = eosio::action_wrapper<"aplswaplog"_n, &amax_one::aplswaplog>;

private: 
    void _claim_reward( const name& to, const asset& recd_apls, bool ads_checked, const string& ads_id, const string& memo );
    void _on_apl_swap_log(
                    const name&         miner,
                    const asset&        recd_apls,
                    const asset&        swap_tokens,
                    const string&       ads_id,
                    const time_point&   created_at);

    string_view _get_ads_id (const string& memo);

    void _add_adsorder(const name& miner, const asset& quantity, const string& ads_id);
    
}; //contract amax.one