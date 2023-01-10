#include <amax.reward/amax.reward.hpp>
#include <eosio/system.hpp>

namespace amax {

using namespace eosio;

static constexpr name ACTIVE_PERM       = "active"_n;

struct amax_token {
      void transfer( const name&    from,
                     const name&    to,
                     const asset&   quantity,
                     const string&  memo );
      using transfer_action = eosio::action_wrapper<"transfer"_n, &amax_token::transfer>;
};

#define TRANSFER_OUT(token_contract, to, quantity, memo)                             \
            amax_token::transfer_action(token_contract, {{get_self(), ACTIVE_PERM}}) \
               .send(get_self(), to, quantity, memo);


namespace db {

    template<typename table, typename Lambda>
    inline void set(table &tbl,  typename table::const_iterator& itr, const eosio::name& emplaced_payer,
            const eosio::name& modified_payer, Lambda&& setter )
   {
        if (itr == tbl.end()) {
            tbl.emplace(emplaced_payer, [&]( auto& p ) {
               setter(p, true);
            });
        } else {
            tbl.modify(itr, modified_payer, [&]( auto& p ) {
               setter(p, false);
            });
        }
    }

    template<typename table, typename Lambda>
    inline void set(table &tbl,  typename table::const_iterator& itr, const eosio::name& emplaced_payer,
               Lambda&& setter )
   {
      set(tbl, itr, emplaced_payer, eosio::same_payer, setter);
   }

}// namespace db

void amax_reward::updatevotes(const name& voter_name, const std::set<name>& producers, double votes) {
    require_auth( SYSTEM_CONTRACT );
    require_auth( voter_name );
    auto now = eosio::current_time_point();

   amax_reward::voter::table voter_tbl(get_self(), get_self().value);
   producer::table producer_tbl(get_self(), get_self().value);
   std::map<name, vote_reward_info> new_producers;
   int64_t new_reward_amount = 0;

   auto voter_itr = voter_tbl.find(voter_name.value);
   if (voter_itr != voter_tbl.end()) {
      // auto old_votes = voter_itr->votes;
      if (voter_itr->votes > 0) {
         for ( const auto& voted_prod : voter_itr->producers) {
            const auto& prod_name = voted_prod.first;
            auto &last_reward_per_vote = voted_prod.second.last_reward_per_vote;
            const auto& prod = producer_tbl.get(prod_name.value, "the voted producer not found");

            producer_tbl.modify( prod, eosio::same_payer, [&]( auto& p ) {
               // allocate rewards
               if (last_reward_per_vote > p.reward_per_vote) {
                  uint64_t amount = (last_reward_per_vote - p.reward_per_vote) * voter_itr->votes;
                  check(p.allocating_rewards.amount >= amount, "producer allocating rewards insufficient");
                  p.allocating_rewards.amount -= amount;
                  p.allocated_rewards.amount += amount;
                  new_reward_amount += amount;
                  p.update_at = now;
               }
               p.votes += votes - voter_itr->votes;
            });

            if (producers.count(prod_name)) {
               new_producers[prod_name].last_reward_per_vote =  prod.reward_per_vote;
            }
         }
      }
   }

   for (const auto &prod_name : producers) {
      if (new_producers.count(prod_name) == 0) {
         auto prod_itr = producer_tbl.find(prod_name.value);
         if (prod_itr != producer_tbl.end()) {
            new_producers[prod_name].last_reward_per_vote = prod_itr->reward_per_vote;
         }
         db::set(producer_tbl, prod_itr, voter_name, [&]( auto& p, bool is_new ) {
            p.owner = prod_name;
            p.votes += votes;
            p.update_at = now;
         });
      }
   }

   db::set(voter_tbl, voter_itr, voter_name, [&]( auto& v, bool is_new ) {
      if (is_new) {
         v.owner = voter_name;
      } else {
         v.unclaimed_rewards.amount += new_reward_amount;
      }
      v.producers = std::move(new_producers);
      v.votes = votes;
      v.update_at = now;
   });

}


void amax_reward::addrewards(const name& producer_name, const asset& quantity) {
   require_auth( SYSTEM_CONTRACT );
   require_auth(producer_name);
   check(is_account(producer_name), "producer account not found");
   check(quantity.symbol == CORE_SYMBOL, "symbol mismatch");
   check(quantity.amount > 0, "quanitty must be positive");
   check(quantity <= _gstate.reward_balance, "reward balance insufficient");
   auto now = eosio::current_time_point();

   producer::table producer_tbl(get_self(), get_self().value);
   auto prod_itr = producer_tbl.find(producer_name.value);
   db::set(producer_tbl, prod_itr, producer_name, producer_name, [&]( auto& p, bool is_new ) {
      if (is_new) {
         p.owner =  producer_name;
      }

      if (p.votes > 0) {
         p.allocating_rewards += quantity;
         p.reward_per_vote += quantity.amount / p.votes;
      } else {
         p.unallocated_rewards += quantity;
      }

      p.update_at = now;
   });

}

void amax_reward::claimrewards(const name& voter_name) {

   require_auth( voter_name );
   auto now = eosio::current_time_point();
   amax_reward::voter::table voter_tbl(get_self(), get_self().value);
   int64_t new_reward_amount = 0;

   auto voter_itr = voter_tbl.find(voter_name.value);
   check(voter_itr != voter_tbl.end(), "voter info not found");
   check(voter_itr->votes > 0, "votes not positive");

   producer::table producer_tbl(get_self(), get_self().value);
   for ( const auto& voted_prod : voter_itr->producers) {
      const auto& prod_name = voted_prod.first;
      auto &last_reward_per_vote = voted_prod.second.last_reward_per_vote;
      auto prod = producer_tbl.get(prod_name.value, "the voted producer not found");

      if (last_reward_per_vote > prod.reward_per_vote) {
         uint64_t amount = (last_reward_per_vote - prod.reward_per_vote) * voter_itr->votes;
         check(prod.allocating_rewards.amount >= amount, "producer allocating rewards insufficient");
         new_reward_amount += amount;

         producer_tbl.modify( prod, eosio::same_payer, [&]( auto& p ) {
            p.allocating_rewards.amount -= amount;
            p.allocated_rewards.amount += amount;
            p.update_at = now;
         });
      }
   }

   auto unclaimed_rewards = voter_itr->unclaimed_rewards;
   unclaimed_rewards.amount += new_reward_amount;
   check(unclaimed_rewards.amount > 0, "no rewards to claim");

   TRANSFER_OUT(CORE_TOKEN, voter_name, unclaimed_rewards, "voted rewards");

   voter_tbl.modify(voter_itr, voter_name, [&]( auto& v) {
      v.unclaimed_rewards.amount = 0;
      v.claimed_rewards += unclaimed_rewards;
      v.update_at = now;
   });
}

void amax_reward::ontransfer(    const name &from,
                                 const name &to,
                                 const asset &quantity,
                                 const string &memo)
{
   if (quantity.symbol == CORE_SYMBOL && from != get_self() && to == get_self()) {
      _gstate.reward_balance += quantity;
      _gstate.total_rewards += quantity;
   }
}

} /// namespace eosio
