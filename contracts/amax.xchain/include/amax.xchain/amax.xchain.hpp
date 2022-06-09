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

    ACTION reqxintoaddr( const name& applicant, const name& applicant_account, const name& base_chain, const uint32_t& mulsign_wallet_id);

    ACTION setaddress( const name& applicant, const name& base_chain, const uint32_t& mulsign_wallet_id, const string& xin_to );

    ACTION mkxinorder(  const name& to, const name& chain_name, const symbol& coin_name, 
                        const string& txid, const string& xin_from, const string& xin_to,
                        const asset& quantity );

    /**
     * checker to confirm xin order
     */
    ACTION checkxinord( const uint64_t& order_id);

    ACTION cancelxinord( const uint64_t& order_id, const string& cancel_reason );

    /**
     * ontransfer, trigger by recipient of transfer()
     * @param quantity - mirrored asset on AMC
     * @param memo - memo format: $addr@$chain@coin_name&order_no
     *               
     */
    [[eosio::on_notify("*::transfer")]] 
    void ontransfer( name from, name to, asset quantity, string memo );

    ACTION setxousent( const uint64_t& order_id, const string& txid, const string& xout_from );

    ACTION setxouconfm( const uint64_t& order_id );

    /**
     * checker to confirm out order
     */
    ACTION checkxouord( const uint64_t& order_id );
    ACTION cancelxouord( const name& account, const uint64_t& order_id, const string& cancel_reason );

    ACTION addchain( const name& account, const name& chain, const name& base_chain, const string& common_xin_account );
    ACTION delchain( const name& account, const name& chain );

    ACTION addcoin( const name& account, const symbol& coin );
    ACTION delcoin( const name& account, const symbol& coin );

    ACTION addchaincoin( const name& account, const name& chain, const symbol& coin, const asset& fee );
    ACTION delchaincoin( const name& account, const name& chain, const symbol& coin );

   private:
    void _check_xin_addr( const name& to, const name& chain_name, const string& xin_to, uint32_t& mulsign_wallet_id );
    checksum256 _get_tixd();
    
};
} //namespace apollo
