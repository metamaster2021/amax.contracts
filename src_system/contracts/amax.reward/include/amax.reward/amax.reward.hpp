#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/privileged.hpp>

#include <string>

#define PP(prop) "," #prop ":", prop
#define PP0(prop) #prop ":", prop
#define PRINT_PROPERTIES(...) eosio::print("{", __VA_ARGS__, "}")

#define CHECK(exp, msg) { if (!(exp)) eosio::check(false, msg); }

#ifndef ASSERT
    #define ASSERT(exp) CHECK(exp, #exp)
#endif

namespace amax {

   using std::string;
   using eosio::contract;
   using eosio::name;
   using eosio::asset;
   using eosio::symbol;
   using eosio::block_timestamp;

   static constexpr name      SYSTEM_CONTRACT   = "amax"_n;
   static constexpr name      CORE_TOKEN        = "amax.token"_n;
   static constexpr symbol    vote_symbol       = symbol("VOTE", 4);
   static const asset         vote_asset_0      = asset(0, vote_symbol);
   static constexpr int128_t  HIGH_PRECISION    = 1'000'000'000'000'000'000; // 10^18

   struct amax_system {
      // Defines new global state parameters.
      struct [[eosio::table("global"), eosio::contract("amax.system")]] amax_global_state : eosio::blockchain_parameters {
         symbol               core_symbol;

         EOSLIB_SERIALIZE_DERIVED( amax_global_state, eosio::blockchain_parameters, (core_symbol) )

         typedef eosio::singleton< "global"_n, amax_global_state >  table;
      };

      static symbol _core_symbol;

      static inline const symbol& core_symbol() {
         if (!_core_symbol.is_valid()) {
            amax_global_state::table tbl(SYSTEM_CONTRACT, SYSTEM_CONTRACT.value);
            auto g = tbl.get();
            _core_symbol = g.core_symbol;
            eosio::check(_core_symbol.is_valid(), "core symbol of system contract is invalid");
         }
         return _core_symbol;
      }
   };


   #define CORE_SYMBOL        amax_system::core_symbol()
   #define CORE_ASSET(amount) asset(amount, CORE_SYMBOL)
   /**
    * The `amax.reward` contract is used as a reward dispatcher contract for amax.system contract.
    *
    */
   class [[eosio::contract("amax.reward")]] amax_reward : public contract {
      public:
         amax_reward( name s, name code, eosio::datastream<const char*> ds ):
               contract(s, code, ds),
               _global(get_self(), get_self().value),
               _voter_tbl(get_self(), get_self().value),
               _producer_tbl(get_self(), get_self().value)
         {
            _gstate  = _global.exists() ? _global.get() : global_state{};
         }

         /**
          * Register producer action.
          * Producer must register before add rewards.
          * Producer must pay for storage.
          *
          * @param producer - account registering to be a producer candidate,
          *
          */
         [[eosio::action]]
         void regproducer( const name& producer );

         /**
          * addvote.
          *
          * @param voter      - the account of voter,
          * @param votes      - votes value,
          */
         [[eosio::action]]
         void addvote( const name& voter, const asset& votes );

         /**
          * subvote.
          *
          * @param voter      - the account of voter,
          * @param votes      - votes value,
          */
         [[eosio::action]]
         void subvote( const name& voter, const asset& votes );


         /**
          * Vote producer action, votes for a set of producers.
          *
          * @param voter - the account to change the voted producers for,
          * @param producers - the list of producers to vote for, a maximum of 30 producers is allowed.
          */
         [[eosio::action]]
         void voteproducer( const name& voter, const std::set<name>& producers );

         /**
          * claim rewards for voter
          *
          * @param voter_name - the account of voter
          */
         [[eosio::action]]
         void claimrewards(const name& voter_name);

        /**
         * Notify by transfer() of xtoken contract
         *
         * @param from - the account to transfer from,
         * @param to - the account to be transferred to,
         * @param quantity - the quantity of tokens to be transferred,
         * @param memo - the memo string to accompany the transaction.
         */
        [[eosio::on_notify("amax.token::transfer")]]
        void ontransfer(   const name &from,
                           const name &to,
                           const asset &quantity,
                           const string &memo);


         struct [[eosio::table("global")]] global_state {
            asset                total_rewards  = CORE_ASSET(0);

            typedef eosio::singleton< "global"_n, global_state >   table;
         };

         /**
          * producer table.
          * scope: contract self
         */
         struct [[eosio::table]] producer {
            name              owner;                                 // PK
            bool              is_registered        = false;          // is initialized
            asset             total_rewards    = CORE_ASSET(0);
            asset             allocating_rewards   = CORE_ASSET(0);
            asset             votes                = vote_asset_0;
            int128_t          rewards_per_vote     = 0;
            block_timestamp   update_at;

            uint64_t primary_key()const { return owner.value; }

            typedef eosio::multi_index< "producers"_n, producer > table;
         };

         struct voted_produer_info {
            int128_t           last_rewards_per_vote         = 0;
         };

         using voted_produer_map = std::map<name, voted_produer_info>;

         /**
          * voter table.
          * scope: contract self
         */
         struct [[eosio::table]] voter {
            name                       owner;
            asset                      votes             = vote_asset_0;
            voted_produer_map          producers;
            asset                      unclaimed_rewards = CORE_ASSET(0);
            asset                      claimed_rewards   = CORE_ASSET(0);
            block_timestamp            update_at;

            uint64_t primary_key()const { return owner.value; }

            typedef eosio::multi_index< "voters"_n, voter > table;
         };

   private:
      global_state::table     _global;
      global_state            _gstate;
      voter::table            _voter_tbl;
      producer::table         _producer_tbl;


      void allocate_producer_rewards(const asset& votes_old, const asset& votes_delta, voted_produer_map& producers, asset &allocated_rewards, const name& new_payer);
      void change_vote(const name& voter, const asset& votes, bool is_adding);
   };

}
