#include <amax.system/amax.system.hpp>
#include <amax.token/amax.token.hpp>

namespace eosiosystem {

   using eosio::current_time_point;
   using eosio::microseconds;
   using eosio::token;

   inline constexpr int64_t power(int64_t base, int64_t exp) {
      int64_t ret = 1;
      while( exp > 0  ) {
         ret *= base; --exp;
      }
      return ret;
   }

   void system_contract::onblock( ignore<block_header> ) {
      using namespace eosio;

      require_auth(get_self());

      // block_timestamp timestamp;
      // name producer;
      // _ds >> timestamp >> producer;
      block_header bh;
      _ds >> bh;
      block_timestamp& timestamp = bh.timestamp;
      name& producer = bh.producer;

      /** until activation, no new rewards are paid */
      if( _gstate.thresh_activated_stake_time == time_point() )
         return;

      /**
       * At startup the initial producer may not be one that is registered / elected
       * and therefore there may be no producer object for them.
       */
      const auto ct = current_time_point();
      if ( _gstate.inflation_start_time != time_point() && ct >= _gstate.inflation_start_time ) {

         int64_t periods = (ct - _gstate.inflation_start_time).count() / (4 * useconds_per_year);
         int64_t inflation_per_block = periods >= 0 && periods < 62 ?
               _gstate.initial_inflation_per_block.amount / power(2, periods) : 0;
         int64_t inflation_per_prod = inflation_per_block / 2;
         if (inflation_per_prod > 0 ) {
            auto prod = _producers.find( producer.value );
            if ( prod != _producers.end() ) {
               _producers.modify( prod, same_payer, [&](auto& p ) {
                     p.unclaimed_rewards.amount += inflation_per_prod;
               });
            }

            backup_block_extension bbe;
            for( size_t i = 0; i < bh.header_extensions.size(); ++i ) {
               const auto& e = bh.header_extensions[i];
               auto id = e.first;
               if (id == backup_block_extension::extension_id()) {
                  datastream<const char*> ext_ds( e.second.data(), e.second.size() );
                  ext_ds >> bbe;
               }
            }
            if (!bbe.is_backup && bbe.previous_backup) {
               auto backup_prod = _producers.find( bbe.previous_backup->producer.value );
               if ( backup_prod != _producers.end() ) {
                  _producers.modify( backup_prod, same_payer, [&](auto& p ) {
                     // TODO: How to determine the inflation reward based on the contribution
                     p.unclaimed_rewards.amount += inflation_per_prod;
                  });
               }
            }
         }
      }

      /// only check and update block producers once every minute
      if( timestamp.slot > _gstate.last_producer_schedule_update.slot + blocks_per_minute ) {
         if (_elect_gstate.is_init()) {
            update_elected_producer_changes( timestamp );
         } else {
            update_elected_producers( timestamp );
         }
         _gstate.last_producer_schedule_update = timestamp;

         /// only process name bid once every day
         if( timestamp.slot > _gstate.last_name_close.slot + blocks_per_day ) {
            name_bid_table bids(get_self(), get_self().value);
            auto idx = bids.get_index<"highbid"_n>();
            auto highest = idx.lower_bound( std::numeric_limits<uint64_t>::max()/2 );
            if( highest != idx.end() &&
                  highest->high_bid > 0 &&
                  (current_time_point() - highest->last_bid_time) > microseconds(useconds_per_day) &&
                  _gstate.thresh_activated_stake_time > time_point() &&
                  (current_time_point() - _gstate.thresh_activated_stake_time) > microseconds(14 * useconds_per_day)
            ) {
               _gstate.last_name_close = timestamp;
               channel_namebid_to_rex( highest->high_bid );
               idx.modify( highest, same_payer, [&]( auto& b ){
                  b.high_bid = -b.high_bid;
               });
            }
         }

      }
   }

   void system_contract::claimrewards( const name& owner ) {
      require_auth( owner );

      const auto& prod = _producers.get( owner.value );
      check( prod.active(), "producer does not have an active key" );

      check( _gstate.thresh_activated_stake_time != time_point(),
                    "cannot claim rewards until the chain is activated (at least 5% of all tokens participate in voting)" );

      const auto ct = current_time_point();
      check( ct >= _gstate.inflation_start_time, "inflation has not yet started");

      // check( false, "inflation and claimrewards are not supported" );

      ASSERT(prod.ext->reward_shared_ratio <= ratio_boost);

      check(prod.unclaimed_rewards.amount > 0, "There are no more rewards to claim");
      int64_t shared_amount = multiply_decimal64(prod.unclaimed_rewards.amount, prod.ext->reward_shared_ratio, ratio_boost);
      ASSERT(shared_amount >= 0 && prod.unclaimed_rewards.amount >= shared_amount);
      uint64_t self_amount = prod.unclaimed_rewards.amount - shared_amount;

      token::issue_action issue_act{ token_account, { {get_self(), active_permission} } };
      issue_act.send( get_self(), prod.unclaimed_rewards, "issue tokens for producer rewards" );

      if (shared_amount > 0) {
         auto shared_quant = asset(shared_amount, prod.unclaimed_rewards.symbol);
         token::transfer_action transfer_act{ token_account, { {get_self(), active_permission} } };
         transfer_act.send( get_self(), reward_account, shared_quant, "producer shared rewards" );
         amax_reward_interface::addrewards_action addrewards_act{ reward_account,
                  { {get_self(), active_permission}, {owner, active_permission} } };
         addrewards_act.send( owner, shared_quant );
      }

      if (self_amount > 0) {
         token::transfer_action transfer_act{ token_account, { {get_self(), active_permission} } };
         transfer_act.send( get_self(), prod.owner, asset(self_amount, prod.unclaimed_rewards.symbol), "producer self rewards" );
      }

      _producers.modify( prod, same_payer, [&](auto& p ) {
            p.unclaimed_rewards.amount = 0;
            p.last_claimed_time = ct;
      });
   }
} //namespace eosiosystem
