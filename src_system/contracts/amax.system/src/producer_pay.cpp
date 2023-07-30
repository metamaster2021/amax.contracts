#include <amax.system/amax.system.hpp>
#include <amax.token/amax.token.hpp>
#include <amax.reward/amax.reward.hpp>

namespace eosiosystem {

   using eosio::current_time_point;
   using eosio::microseconds;
   using eosio::token;
   using amax::amax_reward;

   inline constexpr int64_t power(int64_t base, int64_t exp) {
      int64_t ret = 1;
      while( exp > 0  ) {
         ret *= base; --exp;
      }
      return ret;
   }

   inline void update_reward_info(producer_reward_info& reward_info, const asset& total_rewards, const asset& rewards_per_block) {
      reward_info.total_rewards           = total_rewards;
      reward_info.rewards_per_block       = rewards_per_block;
      if (!reward_info.produced_rewards.is_valid()) {
         reward_info.produced_rewards = asset(0, reward_info.total_rewards.symbol);
      }
      ASSERT(reward_info.total_rewards >= reward_info.produced_rewards);
   }

   inline int64_t calc_having_rewards_per_block(int64_t total_rewards) {
      return total_rewards / 2 / reward_halving_period_blocks;
   }

   inline asset calc_having_rewards_per_block(const producer_reward_info& reward_info) {
      ASSERT(reward_info.total_rewards >= reward_info.produced_rewards)
      int64_t amount = reward_info.total_rewards.amount - reward_info.produced_rewards.amount;
      return asset(calc_having_rewards_per_block( amount >=0 ? amount : 0), reward_info.total_rewards.symbol);
   }

   void system_contract::onblock( ignore<block_header> ) {
      using namespace eosio;

      require_auth(get_self());

      #ifdef APOS_ENABLED
         block_header bh;
         _ds >> bh;
         block_timestamp& timestamp = bh.timestamp;
         name& producer = bh.producer;
      #else
         block_timestamp timestamp;
         name producer;
         _ds >> timestamp >> producer;
      #endif//APOS_ENABLED

      /** until activation, no new rewards are paid */
      if( _gstate.thresh_activated_stake_time == time_point() && !_elect_gstate.is_init())
         return;

      /**
       * At startup the initial producer may not be one that is registered / elected
       * and therefore there may be no producer object for them.
       */
      const auto ct = current_time_point();
      if ( _elect_gstate.is_init() && _gstate.init_reward_start_time != time_point() && ct >= _gstate.init_reward_start_time ) {
         int64_t main_rewards_per_block    = 0;
         auto& main_reward_info              = _elect_gstate.main_reward_info;

         #ifdef APOS_ENABLED
         int64_t backup_rewards_per_block  = 0;
         auto& backup_reward_info         = _elect_gstate.backup_reward_info;
         #endif//APOS_ENABLED

         if (ct >= _gstate.init_reward_end_time) {
            int64_t cur_period_num = 1 + (ct - _gstate.init_reward_end_time).to_seconds() / reward_halving_period_seconds;
            ASSERT(cur_period_num >= _elect_gstate.having_period_num)
            if (cur_period_num > _elect_gstate.having_period_num) {
               _elect_gstate.having_period_num = cur_period_num;
               main_reward_info.rewards_per_block = calc_having_rewards_per_block(main_reward_info);
               #ifdef APOS_ENABLED
               backup_reward_info.rewards_per_block = calc_having_rewards_per_block(backup_reward_info);
               #endif //APOS_ENABLED
            }
         }

         inc_producer_rewards(producer, main_reward_info);

         #ifdef APOS_ENABLED
         if (backup_reward_info.rewards_per_block.amount > 0 ) {
            backup_block_extension bbe;
            for( size_t i = 0; i < bh.header_extensions.size(); ++i ) {
               const auto& e = bh.header_extensions[i];
               auto id = e.first;
               if (id == backup_block_extension::extension_id()) {
                  datastream<const char*> ext_ds( e.second.data(), e.second.size() );
                  ext_ds >> bbe;
               }
            }
            if (!bbe.is_backup && bbe.previous_backup
                  && bbe.previous_backup->contribution >= _elect_gstate.min_backup_reward_contribution) {

               inc_producer_rewards(bbe.previous_backup->producer, backup_reward_info);
            }
         }
         #endif//APOS_ENABLED
      }

      /// only check and update block producers once every minute
      if( timestamp.slot > _gstate.last_producer_schedule_update.slot + blocks_per_minute ) {
         #ifdef APOS_ENABLED
         if (_elect_gstate.is_init()) {
            update_elected_producer_changes( timestamp );
         } else {
            update_elected_producers( timestamp );
         }
         #else
            update_elected_producers( timestamp );
         #endif//APOS_ENABLED
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

   void system_contract::cfgreward( const time_point& init_reward_start_time, const time_point& init_reward_end_time,
                     const asset& main_rewards_per_block, const asset& backup_rewards_per_block )
   {
      require_auth(get_self());

      check( init_reward_end_time >= init_reward_start_time,
         "init_reward_end_time can not be less than init_reward_start_time");

      const auto& core_symb = core_symbol();
      check(main_rewards_per_block.symbol == core_symb && backup_rewards_per_block.symbol == core_symb,
         "rewards symbol mismatch with core symbol");
      check(main_rewards_per_block.amount >= 0  && backup_rewards_per_block.amount >= 0,
         "rewards can not be negative");

      const auto& ct = eosio::current_time_point();

      if (_gstate.init_reward_end_time != time_point() ) {
         check(ct < _gstate.init_reward_end_time, "initializing reward phase has already ended");
      }
      if (_gstate.init_reward_start_time != time_point() && ct >= _gstate.init_reward_start_time ) {
         check( init_reward_start_time == _gstate.init_reward_start_time,
            "can not change init_reward_start_time when initializing reward phase has already started");
      }

      _gstate.init_reward_start_time = init_reward_start_time;
      _gstate.init_reward_end_time = init_reward_end_time;

      auto main_rewards_per_block_max = calc_having_rewards_per_block(total_main_producer_rewards);
      auto backup_rewards_per_block_max = calc_having_rewards_per_block(total_backup_producer_rewards);
      CHECK(main_rewards_per_block.amount <= main_rewards_per_block_max,
         "main_rewards_per_block is larger than " + asset(main_rewards_per_block_max, core_symb).to_string());
      CHECK(backup_rewards_per_block.amount <= backup_rewards_per_block_max,
         "backup_rewards_per_block is larger than " + asset(backup_rewards_per_block_max, core_symb).to_string());

      update_reward_info(_elect_gstate.main_reward_info, asset(total_main_producer_rewards, core_symb), main_rewards_per_block );
      update_reward_info(_elect_gstate.backup_reward_info, asset(total_backup_producer_rewards, core_symb), backup_rewards_per_block );
   }

#ifdef APOS_ENABLED
   void system_contract::cfgcontrib( uint32_t min_backup_reward_contribution )
   {
      require_auth(get_self());

      check( min_backup_reward_contribution <= 10000,
         "min_backup_reward_contribution out of range");
      _elect_gstate.min_backup_reward_contribution = min_backup_reward_contribution;
   }

#endif//APOS_ENABLED

   void system_contract::inc_producer_rewards(const name& producer, producer_reward_info& reward_info) {
      if (reward_info.rewards_per_block.amount <= 0) {
         return;
      }
      auto prod_itr = _producers.find( producer.value );
      if ( prod_itr == _producers.end() ) {
         return;
      }

      auto produced_rewards = reward_info.produced_rewards + reward_info.rewards_per_block;
      if (produced_rewards > reward_info.total_rewards) {
         return;
      }
      _producers.modify( prod_itr, same_payer, [&](auto& p ) {
            p.unclaimed_rewards += reward_info.rewards_per_block;
      });
      reward_info.produced_rewards = produced_rewards;
   }

   void system_contract::claimrewards( const name& submitter, const name& owner ) {
      require_auth( submitter );
      
      const auto& prod = _producers.get( owner.value );
      check( prod.active(), "producer does not have an active key" );

      check(_elect_gstate.is_init(), "election is not initialized");

      const auto ct = current_time_point();

      const auto curr_hours = ( ct.sec_since_epoch() % seconds_per_day ) / 3600;
      CHECK( curr_hours >= 1, "must claim only after 1 AM UTC time" )

      CHECK( prod.ext, "producer hasnot been set via regproducer" )
      ASSERT(prod.ext->reward_shared_ratio <= ratio_boost);

      check(prod.unclaimed_rewards.amount > 0, "There are no more rewards to claim");
      int64_t shared_amount = multiply_decimal64(prod.unclaimed_rewards.amount, prod.ext->reward_shared_ratio, ratio_boost);
      ASSERT(shared_amount >= 0 && prod.unclaimed_rewards.amount >= shared_amount);

      token::issue_action issue_act{ token_account, { {get_self(), active_permission} } };
      issue_act.send( get_self(), prod.unclaimed_rewards, "issue block rewards for producer" );

      token::transfer_action transfer_act{ token_account, { {get_self(), active_permission} } };
      transfer_act.send( get_self(), prod.owner, prod.unclaimed_rewards, "producer block rewards" );

      if (shared_amount > 0) {
         auto shared_quant = asset(shared_amount, prod.unclaimed_rewards.symbol);
         if (!amax_reward::is_producer_registered(reward_account, owner)) {
            amax_reward::regproducer_action reg_act{ reward_account, { {owner, active_permission} } };
            reg_act.send( owner );
         }
         token::transfer_action transfer_act{ token_account, { {owner, active_permission} } };
         transfer_act.send( owner, reward_account, shared_quant, "reward" );
      }

      _producers.modify( prod, same_payer, [&](auto& p ) {
            p.unclaimed_rewards.amount = 0;
            p.last_claimed_time = ct;
      });
   }

   void system_contract::undoreward( const name& owner, const asset& rewards ) {
      require_auth( owner );

      const auto& prod = _producers.get( owner.value );
      CHECK( prod.active(), "producer does not have an active key" )

      CHECK(_elect_gstate.is_init(), "election is not initialized")

      const auto ct = current_time_point();

      CHECK(prod.ext, "producer is not updated by regproducer")

      CHECK(rewards.amount > 0, "rewards must be positive")
      CHECK(prod.unclaimed_rewards >= rewards, "insufficient unclaimed rewards")

      _producers.modify( prod, owner, [&](auto& p ) {
            p.unclaimed_rewards -= rewards;
            p.last_claimed_time = ct;
      });
   }

} //namespace eosiosystem
