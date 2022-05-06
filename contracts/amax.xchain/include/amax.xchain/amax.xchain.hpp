#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <string>
#include <wasm_db.hpp>
#include "amax.xchain/amax.xchain.db.hpp"

namespace amax {

using std::string;
using namespace eosio;
using namespace wasm::db;

/**
 * The `amax.xchain` is Cross-chain (X -> AMAX -> Y) contract
 * 
 */
class [[eosio::contract("amax.xchain")]] xchain : public contract {
private:
   dbc                 _db;
   global_singleton    _global;
   global_t            _gstate;

public:
   using contract::contract;

   xchain(eosio::name receiver, eosio::name code, datastream<const char*> ds):
        _db(_self), contract(receiver, code, ds), _global(_self, _self.value) {
        if (_global.exists()) {
            _gstate = _global.get();

        } else { // first init
            _gstate = global_t{};
            _gstate.admin = _self;
        }
    }

    ~xchain() { _global.set( _gstate, get_self() ); }
   
    ACTION init();
    
    /*
     * Create asset token
     *
     */
    ACTION setaddress( const name& account, const name& base_chain, const string& address );


    ACTION createxin( const name& to, const name& chain, const string& txid, const asset& quantity);

    /**
     * ontransfer, trigger by recipient of transfer()
     * @param quantity - mirrored asset on AMC
     * @param memo - memo format: $addr@$chain
     *               
     */
    [[eosio::on_notify("*::transfer")]] 
    void ontransfer(name from, name to, asset quantity, string memo);

   private:

};
} //namespace apollo
