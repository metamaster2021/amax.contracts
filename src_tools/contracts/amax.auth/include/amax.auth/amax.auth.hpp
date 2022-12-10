#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>
#include <eosio/action.hpp>

#include <string>
#include <amax.auth/amax.auth.db.hpp>
#include <wasm_db.hpp>
#include<amax.system/native.hpp>
#include<amax_recover.hpp>

typedef std::variant<eosio::public_key, amax::string> recover_target_type;
namespace amax {

using std::string;

using namespace wasm::db;
using namespace eosio;

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

/**
 * The `amax.auth` sample system contract defines the structures and actions that allow users to create, issue, and manage tokens for AMAX based blockchains. It demonstrates one way to implement a smart contract which allows for creation and management of tokens. It is possible for one to create a similar contract which suits different needs. However, it is recommended that if one only needs a token with the below listed actions, that one uses the `amax.auth` contract instead of developing their own.
 *
 * The `amax.auth` contract class also implements two useful public static methods: `get_supply` and `get_balance`. The first allows one to check the total supply of a specified token, created by an account and the second allows one to check the balance of a token for a specified account (the token creator account has to be specified as well).
 *
 * The `amax.auth` contract manages the set of tokens, accounts and their corresponding balances, by using two internal multi-index structures: the `accounts` and `stats`. The `accounts` multi-index table holds, for each row, instances of `account` object and the `account` object holds information about the balance of one token. The `accounts` table is scoped to an eosio account, and it keeps the rows indexed based on the token's symbol.  This means that when one queries the `accounts` multi-index table for an account name the result is all the tokens that account holds at the moment.
 *
 * Similarly, the `stats` multi-index table, holds instances of `currency_stats` objects for each row, which contains information about current supply, maximum supply, and the creator account for a symbol token. The `stats` table is scoped to the token symbol.  Therefore, when one queries the `stats` table for a token symbol the result is one single entry/row corresponding to the queried symbol token if it was previously created, or nothing, otherwise.
 */
class [[eosio::contract("amax.auth")]] amax_auth : public contract {
   private:
      dbc                 _dbc;

   public:
      using contract::contract;
  
   amax_auth(eosio::name receiver, eosio::name code, datastream<const char*> ds): contract(receiver, code, ds),
         _dbc(get_self()),
         _global(get_self(), get_self().value)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
    }
    ~amax_auth() { _global.set( _gstate, get_self() ); }

   ACTION init( const name& amax_recover, const name& amax_proxy_contract );

   /**
    * @brief : this is to create a new account with user supplied active key and its owner key comes from amax.proxy
    * 
    * @param admin 
    * @param creator 
    * @param account 
    * @param info 
    * @param active 
    * @return ACTION 
    */
   ACTION newaccount( const name& admin, const name& creator, const name& account, const string& info, const authority& active );

   ACTION createorder(  const uint64_t&            sn,
                        const name&                admin,
                        const name&                account,
                        const bool&                manual_check_required,
                        const uint8_t&             score,
                        const recover_target_type& recover_target
                        );

   /**
    * @brief - this is to set score for user initiated check action. it can be omitted if user fails to meee the condition 
    * 
    * @param admin 
    * @param account 
    * @param order_id 
    * @param score 
    * @return ACTION 
    */
   ACTION setscore( const name& admin, const name& account, const uint64_t& order_id, const uint8_t& score );

   /**
    * @brief 
    * 
    * @param admin 
    * @param account 
    * @param info 
    * @return ACTION 
    */
   ACTION bindinfo( const name& admin, const name& account, const string& info);

   /**
    * @brief 
    * 
    * @param account 
    * @param actions 
    * @return ACTION 
    */
   ACTION setauth( const name& account, const set<name>& actions );

   ACTION delauth( const name& account ) ;

    private:
        global_singleton    _global;
        global_t            _gstate;

   private:
      void _check_action_auth(const name& admin, const name& action_type);

};


} //namespace amax
