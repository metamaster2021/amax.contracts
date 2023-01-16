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

template<typename producer_table>
inline void voter_claim_rewards(amax_reward::voter &voter_info, producer_table &producer_tbl,
      const eosio::time_point &now, const std::set<name>* new_producers, const double* new_votes)
{
   auto& old_prods = voter_info.producers;
   for ( auto voted_prod_itr = old_prods.begin(); voted_prod_itr != old_prods.end(); ) {
      bool erasing = false;
      const auto& prod_name = voted_prod_itr->first;
      const auto &last_rewards_per_vote = voted_prod_itr->second.last_rewards_per_vote;
      const auto& prod = producer_tbl.get(prod_name.value, "the voted producer not found");

      producer_tbl.modify( prod, eosio::same_payer, [&]( auto& p ) {
         if (prod.rewards_per_vote > last_rewards_per_vote && voter_info.votes > 0) {
            int64_t amount = (prod.rewards_per_vote - last_rewards_per_vote) * voter_info.votes;
            if (amount > 0) {
               CHECK(prod.allocating_rewards.amount >= amount, "producer allocating rewards insufficient. "
                     "calc_rewards=" + CORE_ASSET(amount).to_string()
                     + ", allocating_rewards=" + prod.allocating_rewards.to_string()
                     + ", producer=" + prod.owner.to_string()
                     + ", voter=" + voter_info.owner.to_string());
               p.allocating_rewards.amount -= amount;
               p.allocated_rewards.amount += amount;
               voter_info.unclaimed_rewards.amount += amount;
            }
         }
         if (new_producers) { // for update votes
            ASSERT(new_votes != nullptr);

            if (new_producers->count(p.owner)) {
               p.votes += *new_votes - voter_info.votes;
            } else {
               p.votes -=  voter_info.votes;
               erasing = true;
            }
            if (prod_name == "prod.1111141"_n) {
               print("new_votes=", p.votes, "\n");
            }
         }
         p.update_at = now;
      });

      if (!erasing) {
         voted_prod_itr->second.last_rewards_per_vote = prod.rewards_per_vote;
         voted_prod_itr++;
      } else {
         voted_prod_itr = old_prods.erase(voted_prod_itr);
      }
   }
}

void amax_reward::updatevotes(const name& voter_name, const std::set<name>& producers, double votes) {
    require_auth( SYSTEM_CONTRACT );
    require_auth( voter_name );
    auto now = eosio::current_time_point();

   amax_reward::voter::table voter_tbl(get_self(), get_self().value);
   producer::table producer_tbl(get_self(), get_self().value);
   int64_t new_reward_amount = 0;

   auto voter_itr = voter_tbl.find(voter_name.value);
   amax_reward::voter voter_info;
   std::map<name, vote_reward_info> old_producers;
   if (voter_itr != voter_tbl.end()) {
      voter_info = *voter_itr;
      voter_claim_rewards(voter_info, producer_tbl, now, &producers, &votes);
      old_producers = voter_info.producers;
   }

   for (const auto &prod_name : producers) {
      if (!voter_info.producers.count(prod_name)) {
         auto prod_itr = producer_tbl.find(prod_name.value);
         db::set(producer_tbl, prod_itr, voter_name, [&]( auto& p, bool is_new ) {
            if (is_new) {
               p.owner = prod_name;
            }
            p.votes += votes;
            p.update_at = now;

            voter_info.producers[prod_name].last_rewards_per_vote = p.rewards_per_vote;
         });
      }
   }
   voter_info.owner        = voter_name;
   voter_info.votes        = votes;
   voter_info.update_at    = now;

   db::set(voter_tbl, voter_itr, voter_name, voter_name, [&]( auto& v, bool is_new ) {
      v = voter_info;
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
         p.rewards_per_vote += quantity.amount / p.votes;
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
   producer::table producer_tbl(get_self(), get_self().value);
   int64_t new_reward_amount = 0;

   auto voter_itr = voter_tbl.find(voter_name.value);
   check(voter_itr != voter_tbl.end(), "voter info not found");
   check(voter_itr->votes > 0, "votes not positive");

   auto voter_info = *voter_itr;
   voter_claim_rewards(voter_info, producer_tbl, now, nullptr, nullptr);

   check(voter_info.unclaimed_rewards.amount > 0, "no rewards to claim");

   TRANSFER_OUT(CORE_TOKEN, voter_name, voter_info.unclaimed_rewards, "voted rewards");
   voter_info.claimed_rewards += voter_info.unclaimed_rewards;
   voter_info.unclaimed_rewards.amount = 0;
   voter_info.update_at = now;

   voter_tbl.modify(voter_itr, voter_name, [&]( auto& v) {
      v = voter_info;
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
