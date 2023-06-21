#pragma once

#include <eosio/asset.hpp>
#include <eosio/action.hpp>

#include <string>

#include <amax.applybp/amax.applybp.db.hpp>
#include <wasm_db.hpp>

namespace amax {

using std::string;
using std::vector;

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
   NEED_REQUIRED_CHECK  = 20

};

/**
 * The `amax.applybp` sample system contract defines the structures and actions that allow users to create, issue, and manage tokens for AMAX based blockchains. It demonstrates one way to implement a smart contract which allows for creation and management of tokens. It is possible for one to create a similar contract which suits different needs. However, it is recommended that if one only needs a token with the below listed actions, that one uses the `amax.applybp` contract instead of developing their own.
 *
 * The `amax.applybp` contract class also implements two useful public static methods: `get_supply` and `get_balance`. The first allows one to check the total supply of a specified token, created by an account and the second allows one to check the balance of a token for a specified account (the token creator account has to be specified as well).
 *
 * The `amax.applybp` contract manages the set of tokens, accounts and their corresponding balances, by using two internal multi-index structures: the `accounts` and `stats`. The `accounts` multi-index table holds, for each row, instances of `account` object and the `account` object holds information about the balance of one token. The `accounts` table is scoped to an eosio account, and it keeps the rows indexed based on the token's symbol.  This means that when one queries the `accounts` multi-index table for an account name the result is all the tokens that account holds at the moment.
 *
 * Similarly, the `stats` multi-index table, holds instances of `currency_stats` objects for each row, which contains information about current supply, maximum supply, and the creator account for a symbol token. The `stats` table is scoped to the token symbol.  Therefore, when one queries the `stats` table for a token symbol the result is one single entry/row corresponding to the queried symbol token if it was previously created, or nothing, otherwise.
 */
class [[eosio::contract("amax.applybp")]] amax_applybp : public contract {
   
   private:
      dbc                 _dbc;
   public:
      using contract::contract;
  
   amax_applybp(eosio::name receiver, eosio::name code, datastream<const char*> ds): contract(receiver, code, ds),
         _dbc(get_self()),
         _global(get_self(), get_self().value),
         _producer_tbl(get_self(), get_self().value)
    {
        _gstate = _global.exists() ? _global.get() : global_t{};
        
    }

    ~amax_applybp() { _global.set( _gstate, get_self() ); }


   ACTION init( const name& admin);


   ACTION applybp( const name& owner,
                  const string& logo_uri,
                  const string& org_name,
                  const string& org_info,
                  const name& dao_code,
                  const string& reward_shared_plan,
                  const string& manifesto,
                  const string& issuance_plan);


   ACTION updatebp(const name& owner,
                  const string& logo_uri,
                  const string& org_name,
                  const string& org_info,
                  const name& dao_code,
                  const string& reward_shared_plan,
                  const string& manifesto,
                  const string& issuance_plan);

   ACTION addproducer(const name& submiter,
                  const name& owner,
                  const string& logo_uri,
                  const string& org_name,
                  const string& org_info,
                  const name& dao_code,
                  const string& reward_shared_plan,
                  const string& manifesto,
                  const string& issuance_plan);

   ACTION setstatus( const name& submiter, const name& owner, const name& status);

   private:
      global_singleton    _global;
      global_t            _gstate;
      producer_t::table   _producer_tbl;

      void _set_producer(const name& owner,
                  const string& logo_uri,
                  const string& org_name,
                  const string& org_info,
                  const name& dao_code,
                  const string& reward_shared_plan,
                  const string& manifesto,
                  const string& issuance_plan);
};
} //namespace amax
