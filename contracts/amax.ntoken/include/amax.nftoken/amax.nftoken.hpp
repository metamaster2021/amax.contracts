#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>

#include <string>

namespace eosiosystem {
   class system_contract;
}

namespace amax {

using std::string;

/**
 * The `amax.token` sample system contract defines the structures and actions that allow users to create, issue, and manage tokens for AMAX based blockchains. It demonstrates one way to implement a smart contract which allows for creation and management of tokens. It is possible for one to create a similar contract which suits different needs. However, it is recommended that if one only needs a token with the below listed actions, that one uses the `amax.token` contract instead of developing their own.
 * 
 * The `amax.token` contract class also implements two useful public static methods: `get_supply` and `get_balance`. The first allows one to check the total supply of a specified token, created by an account and the second allows one to check the balance of a token for a specified account (the token creator account has to be specified as well).
 * 
 * The `amax.token` contract manages the set of tokens, accounts and their corresponding balances, by using two internal multi-index structures: the `accounts` and `stats`. The `accounts` multi-index table holds, for each row, instances of `account` object and the `account` object holds information about the balance of one token. The `accounts` table is scoped to an eosio account, and it keeps the rows indexed based on the token's symbol.  This means that when one queries the `accounts` multi-index table for an account name the result is all the tokens that account holds at the moment.
 * 
 * Similarly, the `stats` multi-index table, holds instances of `currency_stats` objects for each row, which contains information about current supply, maximum supply, and the creator account for a symbol token. The `stats` table is scoped to the token symbol.  Therefore, when one queries the `stats` table for a token symbol the result is one single entry/row corresponding to the queried symbol token if it was previously created, or nothing, otherwise.
 */
class [[eosio::contract("amax.nftoken")]] token : public contract {
   public:
      using contract::contract;

   /*
   * Update version.
   *
   * This action updates the version string of this SimpleAssets deployment for 3rd party wallets,
               * marketplaces, etc.
   *
   * @param version is version number of SimpleAssetst deployment.
   * @return no return value.
   */
   ACTION updatever( string version );
   using updatever_action = action_wrapper< "updatever"_n, &SimpleAssets::updatever >;

   /*
	 * Change author.
	 *
	 * This action change author. This action replaces author name
	 *
	 * @return no return value.
	 */ 
	ACTION changeauthor( name author, name newauthor, name owner, vector<uint64_t>& assetids, string memo );
	using changeauthor_action = action_wrapper< "changeauthor"_n, &SimpleAssets::changeauthor >;

	/*
    * Transfers one or more assets.
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
   ACTION transfer( name from, name to, vector< uint64_t >& assetids, string memo );
   using transfer_action = action_wrapper< "transfer"_n, &SimpleAssets::transfer >;

   private:
      struct token_info_t {
         uint64_t    id;           //token ID
         string      name;         //token name
         string      symobl;       //token symbol
         string      uri;          //token metadata uri

         token_info_t(const uint64_t& i, const string& name, const string& sy, const string& u):
         id(i), name(n), symobl(sy), uri(u) {};

         // EOSLIB_SERIALIZE( token_info_t, (id)(name)(symbol)(uri) )
      };

      struct [[eosio::table]] nftoken_t {
         token_info_t   token_info;
         name           owner;
         uint64_t       balance;       //token count
         
         uint64_t scope() const { return 0; }
         uint64_t primary_key() const { return token.id; }

         nftoken_t() {}
         nftoken_t(const uint64_t& i, const string& n, const string& sy, const string& u, const name& o, const uint64_t& b):
         token_info(i,n,sy,u),owner(o),balance(b) {}

         EOSLIB_SERIALIZE( nftoken_t, (owner)(token_info)(balance) )

         typedef eosio::multi_index< "nftokens"_n, nftoken_t > tbl_t;
      }
};
}
