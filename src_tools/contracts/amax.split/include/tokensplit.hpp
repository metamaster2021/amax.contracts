#include "tokensplit.db.hpp"

using namespace std;
using namespace wasm::db;

class [[eosio::contract("amax.split")]] tokensplit: public eosio::contract {
private:
    global_singleton    _global;
    global_t            _gstate;

public:
    using contract::contract;

    tokensplit(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        contract(receiver, code, ds),
        _global(get_self(), get_self().value)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~tokensplit() { 
        _global.set( _gstate, get_self() ); 
    }

public:
    ACTION init();

   [[eosio::on_notify("*::transfer")]]
   void ontransfer(const name& from, const name& to, const asset& quant, const string& memo);


}; //contract split