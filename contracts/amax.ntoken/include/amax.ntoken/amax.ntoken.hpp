#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>

#include <string>

#include <amax.ntoken/amax.ntoken_db.hpp>

namespace amax {

using std::string;
using std::vector;

using namespace eosio;

/**
 * The `amax.ntoken` sample system contract defines the structures and actions that allow users to create, issue, and manage tokens for AMAX based blockchains. It demonstrates one way to implement a smart contract which allows for creation and management of tokens. It is possible for one to create a similar contract which suits different needs. However, it is recommended that if one only needs a token with the below listed actions, that one uses the `amax.ntoken` contract instead of developing their own.
 * 
 * The `amax.ntoken` contract class also implements two useful public static methods: `get_supply` and `get_balance`. The first allows one to check the total supply of a specified token, created by an account and the second allows one to check the balance of a token for a specified account (the token creator account has to be specified as well).
 * 
 * The `amax.ntoken` contract manages the set of tokens, accounts and their corresponding balances, by using two internal multi-index structures: the `accounts` and `stats`. The `accounts` multi-index table holds, for each row, instances of `account` object and the `account` object holds information about the balance of one token. The `accounts` table is scoped to an eosio account, and it keeps the rows indexed based on the token's symbol.  This means that when one queries the `accounts` multi-index table for an account name the result is all the tokens that account holds at the moment.
 * 
 * Similarly, the `stats` multi-index table, holds instances of `currency_stats` objects for each row, which contains information about current supply, maximum supply, and the creator account for a symbol token. The `stats` table is scoped to the token symbol.  Therefore, when one queries the `stats` table for a token symbol the result is one single entry/row corresponding to the queried symbol token if it was previously created, or nothing, otherwise.
 */
class [[eosio::contract("amax.ntoken")]] ntoken : public contract {
   public:
      using contract::contract;


   /**
    * @brief Allows `issuer` account to create a token in supply of `maximum_supply`. If validation is successful a new entry in statsta
    * 
    * @param issuer  - the account that creates the token
    * @param maximum_supply - the maximum supply set for the token created
    * @return ACTION 
    */
   ACTION create( const name& issuer, const int64_t& maximum_supply, const uint32_t& parent_tokenid,
                  const string& token_uri, const name& ipowner );

   /**
    * @brief This action issues to `to` account a `quantity` of tokens.
    *
    * @param to - the account to issue tokens to, it must be the same as the issuer,
    * @param quntity - the amount of tokens to be issued,
    * @memo - the memo string that accompanies the token issue transaction.
    */
   ACTION issue( const name& to, const nasset& quantity, const string& memo );

   ACTION retire( const nasset& quantity, const string& memo );
	/**
	 * @brief Transfers one or more assets.
	 * 
    * This action transfers one or more assets by changing scope.
    * Sender's RAM will be charged to transfer asset.
    * Transfer will fail if asset is offered for claim or is delegated.
    *
    * @param from is account who sends the asset.
    * @param to is account of receiver.
    * @param assetids is array of assetid's to transfer.
    * @param memo is transfers comment.
    * @return no return value.
    */
   ACTION transfer( name from, name to, vector< nasset >& assets, string memo );
   using transfer_action = action_wrapper< "transfer"_n, &ntoken::transfer >;
  
   /**
    * @brief fragment a NFT into multiple common or unique NFT pieces
    * 
    * @return ACTION 
    */
   // ACTION fragment();

   private:
   void add_balance( const name& owner, const nasset& value, const name& ram_payer );
   void sub_balance( const name& owner, const nasset& value );
};
} //namespace amax
