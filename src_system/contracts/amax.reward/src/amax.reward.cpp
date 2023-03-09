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

inline static int128_t calc_rewards_per_vote(const int128_t& old_rewards_per_vote, int64_t rewards, int64_t votes) {
   auto  new_rewards_per_vote = old_rewards_per_vote + rewards * HIGH_PRECISION / votes;
   CHECK(new_rewards_per_vote >= old_rewards_per_vote, "calculated rewards_per_vote overflow")
   return new_rewards_per_vote;
}

inline static int64_t calc_voter_rewards(int64_t votes, const int128_t& rewards_per_vote) {
   // with rounding-off method
   int128_t rewards = votes * rewards_per_vote / HIGH_PRECISION;
   CHECK(votes >= 0, "calculated rewards can not be negative")
   CHECK(rewards >= 0 && rewards <= std::numeric_limits<int64_t>::max(),
         "calculated rewards overflow");
   return rewards;
}

inline void allocate_rewards(int64_t votes, const int128_t& last_rewards_per_vote, amax_reward::producer& p, int64_t& new_reward_amount) {
   CHECK(p.rewards_per_vote >= last_rewards_per_vote, "last_rewards_per_vote invalid");
   int128_t reward_per_vote_delta = p.rewards_per_vote - last_rewards_per_vote;
   if (reward_per_vote_delta > 0 && votes > 0) {
      int64_t amount = calc_voter_rewards(votes, reward_per_vote_delta);
      if (amount > 0) {
         CHECK(p.allocating_rewards.amount >= amount, "producer allocating rewards insufficient"
            ", allocating_rewards=" + std::to_string(p.allocating_rewards.amount) +
            ", new_rewards=" + std::to_string(amount) +
            ", producer=" + p.owner.to_string());
         p.allocating_rewards.amount -= amount;
         p.allocated_rewards.amount += amount;
         new_reward_amount += amount;
      }
   }
}

void amax_reward::updatevotes(const name& voter_name, const std::set<name>& producers, int64_t votes) {
    require_auth( SYSTEM_CONTRACT );
    require_auth( voter_name );
    auto now = eosio::current_time_point();

   amax_reward::voter::table voter_tbl(get_self(), get_self().value);
   producer::table producer_tbl(get_self(), get_self().value);
   int64_t new_reward_amount = 0;
   std::map<name, vote_reward_info> new_producers;

   auto voter_itr = voter_tbl.find(voter_name.value);

   struct producer_delta_t {
      int64_t votes                                   = 0;
      std::optional<int128_t> last_rewards_per_vote;
      bool is_new                                     = 0;
   };

   std::map<name, producer_delta_t> producer_deltas;

   if (voter_itr != voter_tbl.end()) {
      for (const auto& p : voter_itr->producers) {
         auto& pd                = producer_deltas[p.first];
         pd.votes               -= voter_itr->votes;
         pd.last_rewards_per_vote  = p.second.last_rewards_per_vote;
      }
   }

   for (const auto& pn : producers) {
      auto& pd = producer_deltas[pn];
      pd.votes   += votes;
      pd.is_new   = true;
   }

   for ( const auto& item : producer_deltas) {
      const auto& prod_name = item.first;
      const auto& pd = item.second;

      auto prod_itr = producer_tbl.find(prod_name.value);
      db::set(producer_tbl, prod_itr, voter_name, [&]( auto& p, bool is_new ) {
         if (is_new) {
            p.owner = prod_name;
         }

         if (pd.last_rewards_per_vote) {
            CHECK(!is_new, "the old producer not found");
            allocate_rewards(voter_itr->votes, *pd.last_rewards_per_vote, p, new_reward_amount);
         }

         p.votes += pd.votes;
         p.update_at = now;

         if (pd.is_new) {
            new_producers[prod_name].last_rewards_per_vote = p.rewards_per_vote;
         }
      });

      /* code */
   }

   db::set(voter_tbl, voter_itr, voter_name, voter_name, [&]( auto& v, bool is_new ) {
      if (is_new) {
         v.owner = voter_name;
      }

      v.votes        = votes;
      v.producers    = new_producers;
      v.update_at    = now;
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
      int128_t old_total_amount = p.get_total_reward_amount();

      if (p.votes > 0) {
         p.allocating_rewards += quantity;
         p.rewards_per_vote = calc_rewards_per_vote(p.rewards_per_vote, quantity.amount, p.votes);
      } else {
         p.unallocated_rewards += quantity;
      }
      int128_t new_total_amount = p.get_total_reward_amount();
      CHECK(new_total_amount >= old_total_amount, "total reward amount overflow")

      p.update_at = now;
   });

}

void amax_reward::claimrewards(const name& voter_name) {
   require_auth( voter_name );
   auto now = eosio::current_time_point();
   amax_reward::voter::table voter_tbl(get_self(), get_self().value);
   producer::table producer_tbl(get_self(), get_self().value);
   std::map<name, vote_reward_info> new_producers;

   auto voter_itr = voter_tbl.find(voter_name.value);
   check(voter_itr != voter_tbl.end(), "voter info not found");
   check(voter_itr->votes > 0, "votes not positive");

   voter_tbl.modify(voter_itr, voter_name, [&]( auto& v) {
      for (auto& voted_prod : v.producers) {
         const auto& prod = producer_tbl.get(voted_prod.first.value, "the voted producer not found");
         producer_tbl.modify( prod, eosio::same_payer, [&]( auto& p ) {
            allocate_rewards(voter_itr->votes, voted_prod.second.last_rewards_per_vote, p, v.unclaimed_rewards.amount);
         });
         voted_prod.second.last_rewards_per_vote = prod.rewards_per_vote;
      }

      check(v.unclaimed_rewards.amount > 0, "no rewards to claim");
      TRANSFER_OUT(CORE_TOKEN, voter_name, v.unclaimed_rewards, "voted rewards");

      v.claimed_rewards += v.unclaimed_rewards;
      v.unclaimed_rewards.amount = 0;
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
