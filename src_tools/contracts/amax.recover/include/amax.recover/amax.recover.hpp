   #pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>
#include <eosio/action.hpp>

#include <string>

#include <amax.recover/amax.recover.db.hpp>
#include <wasm_db.hpp>
#include<amax.system/native.hpp>
#include<amax.system/amax.system.hpp>

namespace amax {

using std::string;
using std::vector;
using eosiosystem::authority;


#define TRANSFER(bank, to, quantity, memo) \
    {	mtoken::transfer_action act{ bank, { {_self, active_perm} } };\
			act.send( _self, to, quantity , memo );}
         
using namespace wasm::db;
using namespace eosio;

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
 * The `amax.recover` sample system contract defines the structures and actions that allow users to create, issue, and manage tokens for AMAX based blockchains. It demonstrates one way to implement a smart contract which allows for creation and management of tokens. It is possible for one to create a similar contract which suits different needs. However, it is recommended that if one only needs a token with the below listed actions, that one uses the `amax.recover` contract instead of developing their own.
 *
 * The `amax.recover` contract class also implements two useful public static methods: `get_supply` and `get_balance`. The first allows one to check the total supply of a specified token, created by an account and the second allows one to check the balance of a token for a specified account (the token creator account has to be specified as well).
 *
 * The `amax.recover` contract manages the set of tokens, accounts and their corresponding balances, by using two internal multi-index structures: the `accounts` and `stats`. The `accounts` multi-index table holds, for each row, instances of `account` object and the `account` object holds information about the balance of one token. The `accounts` table is scoped to an eosio account, and it keeps the rows indexed based on the token's symbol.  This means that when one queries the `accounts` multi-index table for an account name the result is all the tokens that account holds at the moment.
 *
 * Similarly, the `stats` multi-index table, holds instances of `currency_stats` objects for each row, which contains information about current supply, maximum supply, and the creator account for a symbol token. The `stats` table is scoped to the token symbol.  Therefore, when one queries the `stats` table for a token symbol the result is one single entry/row corresponding to the queried symbol token if it was previously created, or nothing, otherwise.
 */
class [[eosio::contract("amax.recover")]] amax_recover : public contract {
   
   private:
      dbc                 _dbc;
   public:
      using contract::contract;
  
   amax_recover(eosio::name receiver, eosio::name code, datastream<const char*> ds): contract(receiver, code, ds),
         _dbc(get_self()),
         _global(get_self(), get_self().value)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }

    ~amax_recover() { _global.set( _gstate, get_self() ); }


   ACTION init( const uint8_t& score_limit) ;

   ACTION bindaccount(  const name& admin, const name& account, const string& number_hash );

   ACTION bindanswer(   const name& admin, const name& account, map<uint8_t, string>& answers );

   ACTION createorder(  const name& admin,
                        const name& account,
                        const string& mobile_hash,
                        const string& recover_target,
                        const bool& manual_check_required) ;

   ACTION chkanswer(    const name& admin,
                        const uint64_t& order_id,
                        const name& account,
                        const int8_t& score);
               

   ACTION chkdid(       const name& admin,
                        const uint64_t& order_id,
                        const name& account,
                        const bool& passed);

   ACTION chkmanual(    const name& admin,
                        const uint64_t& order_id,
                        const name& account,
                        const bool& passed);

   ACTION closeorder(   const name& submitter, const uint64_t& order_id );

   ACTION delorder(     const name& submitter, const uint64_t& order_id );

   ACTION setauditor(   const name& account, const set<name>& actions ) ;
   
   ACTION delauditor(   const name& account );

   ACTION setscore(     const name& audit_type, const int8_t& score );

   ACTION delscore(     const name& audit_type );

   private:
      global_singleton    _global;
      global_t            _gstate;

   private:
      void _check_action_auth(const name& admin, const name& action_type);
      void _get_audit_score( const name& action_type, int8_t& score);
      void _update_authex( const name& account,  const eosio::public_key& pubkey );
      // eosio::public_key string_to_public_key(unsigned int const key_type, std::string const & public_key_str)

};
} //namespace amax
