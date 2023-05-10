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

inline int64_t allocate_rewards(int64_t votes, const int128_t& last_rewards_per_vote, amax_reward::producer& p) {
   CHECK(p.rewards_per_vote >= last_rewards_per_vote, "last_rewards_per_vote invalid");
   int64_t new_reward_amount = 0;
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
   return new_reward_amount;
}

void amax_reward::addvote( const name& voter, const asset& votes ) {
   change_vote(voter, votes, true /* is_adding */);
}


void amax_reward::subvote( const name& voter, const asset& votes ) {
   change_vote(voter, votes, true /* is_adding */);
}

void amax_reward::voteproducer( const name& voter, const std::set<name>& producers ) {

   require_auth( SYSTEM_CONTRACT );
   require_auth( voter );
   auto now = eosio::current_time_point();

   auto voter_itr = _voter_tbl.find(voter.value);

   db::set(_voter_tbl, voter_itr, voter, voter, [&]( auto& v, bool is_new ) {
      if (is_new) {
         v.owner = voter;
      }

      // allocate rewards for old voted producers
      allocate_producer_rewards(v.votes, vote_asset_0, v.producers, v.unclaimed_rewards, voter);

      voted_produer_map added_prods;

      auto voted_prod_itr = v.producers.begin();
      while(voted_prod_itr != v.producers.end()) {
         if (producers.count(voted_prod_itr->first)) {
            added_prods.emplace(*voted_prod_itr);
            voted_prod_itr++;
         } else {
            voted_prod_itr = v.producers.erase(voted_prod_itr);
         }
      }
      // just update new voted prods, because old_votes are 0
      allocate_producer_rewards(vote_asset_0, v.votes, added_prods, v.unclaimed_rewards, voter);
      for (auto& added_prod : added_prods) {
         v.producers.emplace(added_prod);
      }

      v.update_at    = now;
   });
}

void amax_reward::addrewards(const name& producer, const asset& quantity) {

   require_auth(producer);

   check(is_account(producer), "producer account not found");
   check(quantity.symbol == CORE_SYMBOL, "symbol mismatch");
   check(quantity.amount > 0, "quantity must be positive");
   check(quantity <= _gstate.reward_balance, "reward balance insufficient");

   auto now = eosio::current_time_point();
   _gstate.reward_balance -= quantity;

   producer::table _producer_tbl(get_self(), get_self().value);
   auto prod_itr = _producer_tbl.find(producer.value);
   db::set(_producer_tbl, prod_itr, producer, producer, [&]( auto& p, bool is_new ) {
      if (is_new) {
         p.owner =  producer;
      }
      int128_t old_total_amount = p.get_total_reward_amount();

      if (p.votes.amount > 0) {
         p.allocating_rewards += quantity;
         p.rewards_per_vote = calc_rewards_per_vote(p.rewards_per_vote, quantity.amount, p.votes.amount);
      } else {
         p.unallocated_rewards += quantity;
      }
      int128_t new_total_amount = p.get_total_reward_amount();
      CHECK(new_total_amount >= old_total_amount, "total reward amount overflow")

      p.update_at = now;
   });

}

void amax_reward::claimrewards(const name& voter) {
   require_auth( voter );

   auto now = eosio::current_time_point();

   auto voter_itr = _voter_tbl.find(voter.value);
   check(voter_itr != _voter_tbl.end(), "voter info not found");
   check(voter_itr->votes.amount > 0, "voter's votes must be positive");

   _voter_tbl.modify(voter_itr, voter, [&]( auto& v) {

      allocate_producer_rewards(v.votes, vote_asset_0, v.producers, v.unclaimed_rewards, voter);

      check(v.unclaimed_rewards.amount > 0, "no rewards to claim");
      TRANSFER_OUT(CORE_TOKEN, voter, v.unclaimed_rewards, "voted rewards");

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
   if (get_first_receiver() == CORE_TOKEN && quantity.symbol == CORE_SYMBOL && from != get_self() && to == get_self()) {
      _gstate.reward_balance += quantity;
      _gstate.total_rewards += quantity;
   }
}

void amax_reward::change_vote(const name& voter, const asset& votes, bool is_adding) {
   require_auth( SYSTEM_CONTRACT );
   require_auth( voter );

   CHECK(votes.symbol == vote_symbol, "votes symbol mismatch")
   CHECK(votes.amount > 0, "votes must be positive")

   auto now = eosio::current_time_point();
   auto voter_itr = _voter_tbl.find(voter.value);
   db::set(_voter_tbl, voter_itr, voter, voter, [&]( auto& v, bool is_new ) {
      if (is_new) {
         v.owner = voter;
      }

      allocate_producer_rewards(v.votes, votes, v.producers, v.unclaimed_rewards, voter);
      if (is_adding) {
         v.votes        += votes;
      } else {
         CHECK(v.votes >= votes, "voter's votes insufficent")
         v.votes        -= votes;
      }
      CHECK(v.votes.amount >= 0, "voter's votes can not be negtive")
      v.update_at    = now;
   });
}

void amax_reward::allocate_producer_rewards(const asset& votes_old, const asset& votes_delta,
      voted_produer_map& producers, asset &allocated_rewards, const name& new_payer) {

   auto now = eosio::current_time_point();
   for ( auto& voted_prod : producers) {
      const auto& prod_name = voted_prod.first;
      auto& voted_prod_info = voted_prod.second;

      auto prod_itr = _producer_tbl.find(prod_name.value);
      db::set(_producer_tbl, prod_itr, new_payer, same_payer, [&]( auto& p, bool is_new ) {
         if (is_new) {
            p.owner = prod_name;
         }

         if (votes_old.amount > 0) {
            allocated_rewards.amount += allocate_rewards(votes_old.amount, voted_prod_info.last_rewards_per_vote, p);
            // TODO: check allocated_rewards: overflow?
         }

         p.votes += votes_delta;
         CHECK(p.votes.amount >= 0, "producer votes can not be negtive")
         p.update_at = now;

         voted_prod_info.last_rewards_per_vote = p.rewards_per_vote; // update last_rewards_per_vote for voted_prod
      });

   }
}

} /// namespace eosio
