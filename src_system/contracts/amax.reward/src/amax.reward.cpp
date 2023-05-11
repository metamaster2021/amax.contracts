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
   ASSERT(rewards >= 0 && votes >= 0);
   int128_t  new_rewards_per_vote = old_rewards_per_vote;
   if (rewards > 0 && votes > 0) {
      new_rewards_per_vote = old_rewards_per_vote + rewards * HIGH_PRECISION / votes;
      CHECK(new_rewards_per_vote >= old_rewards_per_vote, "calculated rewards_per_vote overflow")
   }
   return new_rewards_per_vote;
}

inline static asset calc_voter_rewards(const asset& votes, const int128_t& rewards_per_vote) {
   // with rounding-off method
   int128_t rewards = votes.amount * rewards_per_vote / HIGH_PRECISION;
   CHECK(rewards >= 0 && rewards <= std::numeric_limits<int64_t>::max(),
         "calculated rewards overflow");
   return CORE_ASSET(rewards);
}

void amax_reward::regproducer( const name& producer ) {

   require_auth(producer);

   check(is_account(producer), "producer account not found");

   auto now = eosio::current_time_point();

   auto prod_itr = _producer_tbl.find(producer.value);
   db::set(_producer_tbl, prod_itr, producer, producer, [&]( auto& p, bool is_new ) {
      if (is_new) {
         p.owner =  producer;
      }
      p.is_registered = true;
      p.update_at = now;
   });
}

void amax_reward::addvote( const name& voter, const asset& votes ) {
   change_vote(voter, votes, true /* is_adding */);
}


void amax_reward::subvote( const name& voter, const asset& votes ) {
   change_vote(voter, votes, true /* is_adding */);
}

void amax_reward::voteproducer( const name& voter, const std::vector<name>& producers ) {

   require_auth( SYSTEM_CONTRACT );
   require_auth( voter );
   for( size_t i = 1; i < producers.size(); ++i ) {
      check( producers[i - 1] < producers[i], "producer votes must be uniqued and sorted" );
   }

   auto now = eosio::current_time_point();

   auto voter_itr = _voter_tbl.find(voter.value);
   db::set(_voter_tbl, voter_itr, voter, voter, [&]( auto& v, bool is_new ) {
      if (is_new) {
         v.owner = voter;
      }

      // allocate rewards for old voted producers
      allocate_producer_rewards(v.votes, vote_asset_0, v.producers, v.unclaimed_rewards, voter);

      voted_produer_map added_prods;

      auto new_prod_itr = producers.begin();
      auto old_prod_itr = v.producers.begin();
      while( old_prod_itr != v.producers.end() || new_prod_itr != producers.end() ) {

         if (old_prod_itr != v.producers.end() && new_prod_itr != producers.end()) {
            if ( old_prod_itr->first < (*new_prod_itr) ) {
               // old is discarded, new is processed in next loop
               old_prod_itr = v.producers.erase(old_prod_itr);
            }else if ( (*new_prod_itr) < old_prod_itr->first ) {
               // old is processed in next loop, new is added to added_prods
               added_prods[*new_prod_itr] = {};
               new_prod_itr++;
            } else { // old_prod_itr->first == (*new_prod_itr))
               // old is keeped in v.producers, new is discarded
               old_prod_itr++;
               new_prod_itr++;
            }

         } else if (old_prod_itr != v.producers.end()) {
            // no new, old is discarded
            old_prod_itr = v.producers.erase(old_prod_itr);
         } else { // new_prod_itr != producers.end()
            // no old, new is added to added_prods
            added_prods[*new_prod_itr] = {};
            new_prod_itr++;
         }
      }

      // update new added prods
      allocate_producer_rewards(vote_asset_0, v.votes, added_prods, v.unclaimed_rewards, voter);
      for (auto& added_prod : added_prods) {
         v.producers.emplace(added_prod);
      }

      v.update_at    = now;
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
      _gstate.total_rewards += quantity;
      _global.set(_gstate, get_self());

      auto prod_itr = _producer_tbl.find(from.value);
      check(prod_itr != _producer_tbl.end(), "producer(from) not found");
      check(!prod_itr->is_registered, "producer(from) not registered");
      _producer_tbl.modify(prod_itr, same_payer, [&]( auto& p ) {
         p.total_rewards         += quantity;
         p.allocating_rewards   += quantity;
         p.rewards_per_vote      = calc_rewards_per_vote(p.rewards_per_vote, quantity.amount, p.votes.amount);
         p.update_at = eosio::current_time_point();
      });
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
      auto& last_rewards_per_vote = voted_prod.second.last_rewards_per_vote; // will be updated below

      auto prod_itr = _producer_tbl.find(prod_name.value);
      db::set(_producer_tbl, prod_itr, new_payer, same_payer, [&]( auto& p, bool is_new ) {
         if (is_new) {
            p.owner = prod_name;
         }

         CHECK(p.rewards_per_vote >= last_rewards_per_vote, "last_rewards_per_vote invalid");
         int128_t rewards_per_vote_delta = p.rewards_per_vote - last_rewards_per_vote;
         if (rewards_per_vote_delta > 0 && votes_old.amount > 0) {
            asset new_rewards = calc_voter_rewards(votes_old, rewards_per_vote_delta);
            CHECK(p.allocating_rewards >= new_rewards, "producer allocating rewards insufficient");
            p.allocating_rewards -= new_rewards;
            allocated_rewards += new_rewards; // update allocated_rewards for voter
         }

         p.votes += votes_delta;
         CHECK(p.votes.amount >= 0, "producer votes can not be negtive")
         p.update_at = now;

         last_rewards_per_vote = p.rewards_per_vote; // update for voted_prod
      });

   }
}

} /// namespace eosio
