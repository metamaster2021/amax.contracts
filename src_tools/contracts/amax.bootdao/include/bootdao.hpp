#include "bootdao_db.hpp"

using namespace std;
using namespace wasm::db;

class [[eosio::contract("amax.bootdao")]] bootdao: public eosio::contract {
private:
    global_singleton    _global;
    global_t            _gstate;

public:
    using contract::contract;

    bootdao(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds),
        _global(get_self(), get_self().value)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~bootdao() { _global.set( _gstate, get_self() ); }

public:
    ACTION init();

}; //contract bootdao