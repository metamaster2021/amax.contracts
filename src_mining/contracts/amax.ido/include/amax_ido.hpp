#include "amax_ido.db.hpp"

using namespace std;
using namespace wasm::db;

class [[eosio::contract("amax.ido")]] amax_ido: public eosio::contract {
private:
    global_singleton    _global;
    global_t            _gstate;

public:
    using contract::contract;

    amax_ido(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds),
        _global(get_self(), get_self().value)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~amax_ido() { _global.set( _gstate, get_self() ); }
    
    [[eosio::action]] void init(const name& admin);
    [[eosio::action]] void setprice(const asset &price);

    /**
     * ontransfer, trigger by recipient of transfer()
     *  @param from - issuer
     *  @param to   - must be contract self
     *  @param quantity - issued quantity
     *  @param memo - memo format:
     */
    [[eosio::on_notify("*::transfer")]] void ontransfer(name from, name to, asset quantity, string memo);

}; //contract amax.ido