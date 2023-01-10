#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/singleton.hpp>
#include <eosio/privileged.hpp>

#include <string>

namespace amax {

   using std::string;
   using eosio::contract;
   using eosio::name;
   using eosio::asset;
   using eosio::symbol;
   using eosio::block_timestamp;

   static constexpr name      SYSTEM_CONTRACT = "amax"_n;
   static constexpr name      CORE_TOKEN = "amax.token"_n;

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
               contract(s, code, ds), _global(get_self(), get_self().value)
         {
            _gstate  = _global.exists() ? _global.get() : global_state{};
         }

         ~amax_reward() {
            _global.set(_gstate, get_self());
         }

         /**
          * update votes.
          *
          * @param voter_name - the account of voter,
          * @param producers - producer list
          * @param old_votes - old votes value,
          * @param new_votes - new votes value.
          */
         [[eosio::action]]
         void updatevotes(const name& voter_name, const std::set<name>& producers, double votes);

         /**
          * add reward for producer, deduct from the reward balance
          *
          * @param producer_name - the account of producer,
          * @param quantity - reward quantity
          */
         [[eosio::action]]
         void addrewards(const name& producer_name, const asset& quantity);


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


      private:


         struct [[eosio::table("global")]] global_state {
            asset                reward_balance = CORE_ASSET(0);
            asset                total_rewards  = CORE_ASSET(0);

            typedef eosio::singleton< "global"_n, global_state >   table;
         };

         struct [[eosio::table]] producer {
            name              owner;
            asset             unallocated_rewards  = CORE_ASSET(0);
            asset             allocating_rewards   = CORE_ASSET(0);
            asset             allocated_rewards    = CORE_ASSET(0);
            double            votes                = 0;
            double            reward_per_vote      = 0;
            block_timestamp   update_at;

            uint64_t primary_key()const { return owner.value; }

            typedef eosio::multi_index< "producers"_n, producer > table;
         };


         struct vote_reward_info {
            double               last_reward_per_vote          = 0;
         };

         struct [[eosio::table]] voter {
            name                             owner;
            double                           votes             = 0;
            std::map<name, vote_reward_info> producers;
            asset                            unclaimed_rewards = CORE_ASSET(0);
            asset                            claimed_rewards   = CORE_ASSET(0);
            block_timestamp                  update_at;

            uint64_t primary_key()const { return owner.value; }

            typedef eosio::multi_index< "voters"_n, voter > table;
         };

   private:
      global_state::table     _global;
      global_state            _gstate;

   };

}
