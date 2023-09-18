#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>
#include <eosio/action.hpp>

#include <string>

#include <xchain.owner/xchain.owner.db.hpp>
#include <wasm_db.hpp>
#include <amax_system.hpp>

namespace amax {

using std::string;
using std::vector;


#define TRANSFER(bank, to, quantity, memo) \
    {	mtoken::transfer_action act{ bank, { {_self, active_perm} } };\
			act.send( _self, to, quantity , memo );}
         
using namespace wasm::db;
using namespace eosio;
using namespace wasm;
#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

static constexpr name      NFT_BANK    = "did.ntoken"_n;
static constexpr eosio::name active_perm{"active"_n};


enum class err: uint8_t {
   NONE                 = 0,
   RECORD_NOT_FOUND     = 1,
   RECORD_EXISTING      = 2,
   SYMBOL_MISMATCH      = 4,
   PARAM_ERROR          = 5,
   MEMO_FORMAT_ERROR    = 6,
   PAUSED               = 7,
   NO_AUTH              = 8,
   NOT_POSITIVE         = 9,
   NOT_STARTED          = 10,
   OVERSIZED            = 11,
   TIME_EXPIRED         = 12,
   NOTIFY_UNRELATED     = 13,
   ACTION_REDUNDANT     = 14,
   ACCOUNT_INVALID      = 15,
   FEE_INSUFFICIENT     = 16,
   FIRST_CREATOR        = 17,
   STATUS_ERROR         = 18,
   SCORE_NOT_ENOUGH     = 19,
   NEED_MANUAL_CHECK    = 20
};
namespace vendor_info_status {
    static constexpr eosio::name RUNNING            = "running"_n;
    static constexpr eosio::name STOP               = "stop"_n;
};

/**
 * The `xchain.owner` sample system contract defines the structures and actions that allow users to create, issue, and manage tokens for AMAX based blockchains. It demonstrates one way to implement a smart contract which allows for creation and management of tokens. It is possible for one to create a similar contract which suits different needs. However, it is recommended that if one only needs a token with the below listed actions, that one uses the `xchain.owner` contract instead of developing their own.
 *
 * The `xchain.owner` contract class also implements two useful public static methods: `get_supply` and `get_balance`. The first allows one to check the total supply of a specified token, created by an account and the second allows one to check the balance of a token for a specified account (the token creator account has to be specified as well).
 *
 * The `xchain.owner` contract manages the set of tokens, accounts and their corresponding balances, by using two internal multi-index structures: the `accounts` and `stats`. The `accounts` multi-index table holds, for each row, instances of `account` object and the `account` object holds information about the balance of one token. The `accounts` table is scoped to an eosio account, and it keeps the rows indexed based on the token's symbol.  This means that when one queries the `accounts` multi-index table for an account name the result is all the tokens that account holds at the moment.
 *
 * Similarly, the `stats` multi-index table, holds instances of `currency_stats` objects for each row, which contains information about current supply, maximum supply, and the creator account for a symbol token. The `stats` table is scoped to the token symbol.  Therefore, when one queries the `stats` table for a token symbol the result is one single entry/row corresponding to the queried symbol token if it was previously created, or nothing, otherwise.
 */
class [[eosio::contract("xchain.owner")]] xchain_owner : public contract {
   
   private:
      dbc                 _dbc;
   public:
      using contract::contract;
  
   xchain_owner(eosio::name receiver, eosio::name code, datastream<const char*> ds): contract(receiver, code, ds),
         _dbc(get_self()),
         _global(get_self(), get_self().value)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }
    ~xchain_owner() { _global.set( _gstate, get_self() ); }

   ACTION init(         const name& admin, 
                        const name& oracle_maker, 
                        const name& oracle_checker, 
                        const asset& stake_net_quantity, 
                        const asset& stake_cpu_quantity) {
      // CHECKC( has_auth(_self),  err::NO_AUTH, "no auth to operate" )      

      _gstate.admin                 = admin;
      _gstate.oracle_makers.insert( oracle_maker );
      _gstate.oracle_checkers.insert( oracle_checker );
      _gstate.stake_net_quantity    = stake_net_quantity;
      _gstate.stake_cpu_quantity    = stake_cpu_quantity;

   }

   ACTION proposebind(  const name& oracle_maker, 
                        const name& xchain, 
                        const string& xchain_txid, 
                        const string& xchain_pubkey, 
                        const eosio::public_key& pubkey, 
                        const name& account );

   ACTION approvebind(  const name& oracle_checker, 
                        const name& xchain, 
                        const string& xchain_txid );

   ACTION setoracle( const name& oracle, const bool& is_maker, const bool& to_add ) {
      require_auth( _gstate.admin );
      check( is_account( oracle ), oracle.to_string() + ": invalid account" );

      if( is_maker ) {
         bool found = ( _gstate.oracle_makers.find( oracle ) != _gstate.oracle_makers.end() );

         if (to_add) {
            check( !found, "oracle already added" );
            _gstate.oracle_makers.insert( oracle );

         } else {
            check( found, "oracle not found" );
            _gstate.oracle_makers.erase( oracle );
         }
      }
         
      else {
         bool found = ( _gstate.oracle_checkers.find( oracle ) != _gstate.oracle_checkers.end() );

         if (to_add) {
            check( !found, "oracle already added" );
            _gstate.oracle_checkers.insert( oracle );

         } else {
            check( found, "oracle not found" );
            _gstate.oracle_checkers.erase( oracle );
         }
      }
   }

    private:
        global_singleton    _global;
        global_t            _gstate;

   private:
      void _check_action_auth(const name& admin, const name& action_type);
   
};


} //namespace amax
