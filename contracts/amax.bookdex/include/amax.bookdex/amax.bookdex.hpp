#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>
#include <string>
#include <amax.bookdex/amax.bookdex_db.hpp>

namespace amax {

using std::string;
using std::vector;

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
   STATUS_ERROR         = 18

};

/**
 * The `amax.dexbook` sample system contract defines the structures and actions that allow users to create, issue, and manage tokens for AMAX based blockchains. It demonstrates one way to implement a smart contract which allows for creation and management of tokens. It is possible for one to create a similar contract which suits different needs. However, it is recommended that if one only needs a token with the below listed actions, that one uses the `amax.dexbook` contract instead of developing their own.
 * 
 * The `amax.dexbook` contract class also implements two useful public static methods: `get_supply` and `get_balance`. The first allows one to check the total supply of a specified token, created by an account and the second allows one to check the balance of a token for a specified account (the token creator account has to be specified as well).
 * 
 * The `amax.dexbook` contract manages the set of tokens, accounts and their corresponding balances, by using two internal multi-index structures: the `accounts` and `stats`. The `accounts` multi-index table holds, for each row, instances of `account` object and the `account` object holds information about the balance of one token. The `accounts` table is scoped to an eosio account, and it keeps the rows indexed based on the token's symbol.  This means that when one queries the `accounts` multi-index table for an account name the result is all the tokens that account holds at the moment.
 * 
 * Similarly, the `stats` multi-index table, holds instances of `currency_stats` objects for each row, which contains information about current supply, maximum supply, and the creator account for a symbol token. The `stats` table is scoped to the token symbol.  Therefore, when one queries the `stats` table for a token symbol the result is one single entry/row corresponding to the queried symbol token if it was previously created, or nothing, otherwise.
 */
class [[eosio::contract("amax.bookdex")]] bookdex : public contract {
   public:
      using contract::contract;

   [[eosio::on_notify("*::transfer")]]
   void ontransfer(const name& from, const name& to, const asset& quantity, const string& memo);
 
   ACTION addprice();

   private:
   void process_limit_buy(  const trade_pair_t& trade_pair, baseoffer_idx& offers, 
                            const price_s& bid_price, const name& to, asset& quantity );
   void process_market_buy( const trade_pair_t& trade_pair, baseoffer_idx& offers, 
                            const float& slippage, const name& to, asset& quantity );
   void process_limit_sell( const trade_pair_t& trade_pair, quoteoffer_idx& offers, 
                            const price_s& ask_price, const name& to, asset& quantity );
   void process_market_sell(const trade_pair_t& trade_pair, quoteoffer_idx& offers, 
                           const float& slippage, const name& to, asset& quantity );

};
} //namespace amax
