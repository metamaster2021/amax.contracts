#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <string>
#include <wasm_db.hpp>
#include "amax.xchain/amax.token.hpp"
#include "amax.xchain/amax.xchain.db.hpp"

namespace amax {

using std::string;
using namespace eosio;
using namespace wasm::db;

/**
 * The `amax.xchain` is Cross-chain (X -> AMAX -> Y) contract
 * 
 */

#define TRANSFER(bank, to, quantity, memo) \
    {	token::transfer_action act{ bank, { {_self, active_perm} } };\
			act.send( _self, to, quantity , memo );}


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
   
    ACTION init( const name& admin, const name& maker, const name& checker, const name& fee_collector );

    ACTION reqxintoaddr( const name& account, const name& base_chain );

    ACTION setaddress( const name& applicant, const name& base_chain, const string& xin_to );

    ACTION mkxinorder(  const name& to, const name& chain_name, const symbol& coin_name, 
                        const string& txid, const string& xin_from, const string& xin_to,
                        const asset& quantity);

    /**
     * checker to confirm xin order
     */
    ACTION chkxinorder( const uint64_t& id);

    ACTION cslxinorder( const uint64_t& id, const string& cancel_reason );

    /**
     * ontransfer, trigger by recipient of transfer()
     * @param quantity - mirrored asset on AMC
     * @param memo - memo format: $addr@$chain@coin_name&order_no
     *               
     */
    [[eosio::on_notify("*::transfer")]] 
    void ontransfer(name from, name to, asset quantity, string memo);

    ACTION onpay( const name& account, const uint64_t& id, const string& txid, const string& payno, const string& xout_from );

    ACTION onpaysucc( const name& account, const uint64_t& id );

    /**
     * checker to confirm out order
     */
    ACTION chkxoutorder( const uint64_t& id );

    ACTION cancelxout( const name& account, const uint64_t& id );

    ACTION addchain( const name& chain, const bool is_basechain, const string& common_xin_account );

    ACTION delchain( const name& chain );

    ACTION addcoin( const symbol& coin );

    ACTION delcoin( const symbol& coin );

    ACTION addchaincoin( const name& chain, const symbol& coin, const asset& fee );

    ACTION delchaincoin( const name& chain, const symbol& coin );

    ACTION deltable();
   private:
    
};
} //namespace apollo
