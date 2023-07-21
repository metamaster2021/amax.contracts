#include <eosio/crypto.hpp>
#include <eosio/datastream.hpp>
#include <eosio/eosio.hpp>
#include <eosio/multi_index.hpp>
#include <eosio/permission.hpp>
#include <eosio/privileged.hpp>
#include <eosio/serialize.hpp>
#include <eosio/singleton.hpp>

#include <amax.system/amax.system.hpp>
#include <amax.token/amax.token.hpp>
#include <amax.reward/amax.reward.hpp>

#include <type_traits>
#include <limits>
#include <set>
#include <algorithm>
#include <cmath>

// #define TRACE_PRODUCER_CHANGES 1

namespace eosio {

   inline bool operator == ( const key_weight& lhs, const key_weight& rhs ) {
      return tie( lhs.key, lhs.weight ) == tie( rhs.key, rhs.weight );
   }
   inline bool operator == ( const block_signing_authority_v0& lhs, const block_signing_authority_v0& rhs ) {
      return tie( lhs.threshold, lhs.keys ) == tie( rhs.threshold, rhs.keys );
   }
   inline bool operator != ( const block_signing_authority_v0& lhs, const block_signing_authority_v0& rhs ) {
      return !(lhs == rhs);
   }
}

namespace eosiosystem {

   using eosio::const_mem_fun;
   using eosio::current_time_point;
   using eosio::current_block_time;
   using eosio::indexed_by;
   using eosio::microseconds;
   using eosio::seconds;
   using eosio::singleton;
   #ifdef APOS_ENABLED
   using eosio::producer_authority_add;
   using eosio::producer_authority_modify;
   using eosio::producer_authority_del;
   using eosio::producer_change_map;
   #endif//APOS_ENABLED
   using eosio::print;
   using std::to_string;
   using std::string;
   using eosio::token;
   using amax::amax_reward;

   inline bool operator == ( const eosio::key_weight& lhs, const eosio::key_weight& rhs ) {
      return tie( lhs.key, lhs.weight ) == tie( rhs.key, rhs.weight );
   }

#ifdef APOS_ENABLED

   namespace producer_change_helper {

      using change_map_t = std::map<name, eosio::producer_change_record>;
      void add( change_map_t &changes, const name& producer_name,
                const eosio::block_signing_authority  producer_authority) {
         auto itr = changes.find(producer_name);
         if (itr != changes.end()) {
            auto op = (eosio::producer_change_operation)itr->second.index();
            switch (op) {
               case eosio::producer_change_operation::del :
                  itr->second = eosio::producer_authority_modify{producer_authority};
                  break;
               default:
                  CHECK(false, "the old change type can not be " + std::to_string((uint8_t)op) + " when add prod change: " + producer_name.to_string())
                  break;
            }
         } else {
            changes.emplace(producer_name, eosio::producer_authority_add{producer_authority});
         }
      }

      inline void add(change_map_t &changes, const producer_elected_info& producer) {
         add(changes, producer.name, producer.authority);
      }

      void modify( change_map_t &changes, const name& producer_name,
                   const eosio::block_signing_authority  producer_authority) {
         auto itr = changes.find(producer_name);
         if (itr != changes.end()) {
            auto op = (eosio::producer_change_operation)itr->second.index();
            switch (op) {
               case eosio::producer_change_operation::add :
                  std::get<0>(itr->second).authority = producer_authority;
                  break;
               case eosio::producer_change_operation::modify :
                  std::get<1>(itr->second).authority = producer_authority;
                  break;
               default:
                  CHECK(false, "the old change type can not be " + std::to_string((uint8_t)op) + " when modify prod change: " + producer_name.to_string())
                  break;
            }
         } else {
            changes.emplace(producer_name, eosio::producer_authority_modify{producer_authority});
         }
      }

      void modify(change_map_t &changes, const producer_elected_info& producer) {
        modify(changes, producer.name, producer.authority);
      }

      void del(change_map_t &changes, const name& producer_name) {
         auto itr = changes.find(producer_name);
         if (itr != changes.end()) {
            auto op = (eosio::producer_change_operation)itr->second.index();
            switch (op) {
               case eosio::producer_change_operation::add :
                  changes.erase(itr);
                  break;
               case eosio::producer_change_operation::modify :
                  itr->second = eosio::producer_authority_del{};
                  break;
               default:
                  CHECK(false, "the old change type can not be " + std::to_string((uint8_t)op) + " when del prod change: " + producer_name.to_string())
                  break;
            }
         } else {
            changes.emplace(producer_name, eosio::producer_authority_del{});
         }
      }

      void del(change_map_t &changes, const producer_elected_info& producer) {
         del(changes, producer.name);
      }

      void merge(const producer_change_map& src, producer_change_map& dest) {
         if (src.clear_existed) {
            dest = src;
            return;
         }
         for (const auto& c : src.changes) {
            std::visit(
               overloaded {
                  [&prod_name=c.first, &dest_changes=dest.changes](const producer_authority_add& v) {
                     add(dest_changes, prod_name, *v.authority);
                  },
                  [&prod_name=c.first, &dest_changes=dest.changes](const producer_authority_modify& v) {
                     modify(dest_changes, prod_name, *v.authority);
                  },
                  [&prod_name=c.first, &dest_changes=dest.changes](const producer_authority_del& v) {
                     del(dest_changes, prod_name);
                  }},
               c.second);
         }
         dest.producer_count = src.producer_count;
      }

      void merge(const proposed_producer_changes& src, proposed_producer_changes& dest) {
         merge(src.main_changes, dest.main_changes);
         merge(src.backup_changes, dest.backup_changes);
      }
   }

   namespace queue_helper {

      template<typename index_t>
      auto get_pos_itr(index_t &idx, const producer_elected_info &prod, const char* title) {
         auto itr = idx.lower_bound(producer_info::by_elected_prod(prod.name, prod.is_active, prod.elected_votes));
         CHECK(itr != idx.end() && itr->owner == prod.name, "producer elected position not found");
         ASSERT(itr->get_elected_votes() == prod.elected_votes);
         return itr;
      }

      template<typename index_t>
      void fetch_prev(index_t &idx, const producer_elected_info &tail, producer_elected_info &prev, bool checking, const char* title) {
         auto itr = get_pos_itr(idx, tail, title);
         auto begin = idx.begin();
         check(begin != idx.end(), "electedprod index of producer table is empty");
         if (itr != begin) {
            itr--;
            itr->get_elected_info(prev);
            ASSERT(tail < prev);
            #ifdef TRACE_PRODUCER_CHANGES
            eosio::print(title, " updated: ", itr->owner, ":", itr->get_elect_votes(), "\n");
            #endif//TRACE_PRODUCER_CHANGES
         } else {
            if (checking) {
               CHECK(false, string(title) + " not found! tail: " + tail.name.to_string() + ":" + tail.elected_votes.to_string())
            }
            #ifdef TRACE_PRODUCER_CHANGES
            eosio::print(title, " cleared\n");
            #endif//TRACE_PRODUCER_CHANGES
            prev.clear();
         }
      }

      template<typename index_t>
      void fetch_next(index_t &idx, const producer_elected_info &tail, producer_elected_info &next, bool check_found, const char* title) {
         auto itr = get_pos_itr(idx, tail, title);
         itr++;
         if (itr != idx.end() && itr->ext) {
            itr->get_elected_info(next);
            ASSERT(next < tail);
            #ifdef TRACE_PRODUCER_CHANGES
            eosio::print(title, " updated: ", itr->owner, ":", itr->get_elected_votes(), "\n");
            #endif//TRACE_PRODUCER_CHANGES
         } else {
            if (check_found) {
               CHECK(false, string(title) + " not found! tail: " + tail.name.to_string() + ":" + tail.elected_votes.to_string())
            }
            #ifdef TRACE_PRODUCER_CHANGES
            eosio::print(title, " cleared\n");
            #endif//TRACE_PRODUCER_CHANGES
            next.clear();
         }
      }

   }

#endif//APOS_ENABLED

   void system_contract::upgradevote() {

      require_auth( get_self() );

      // check(_elect_gstate.elected_version == 0, "new voting strategy has been enabled");
      auto ct = current_time_point();

      _elect_gstate.elected_version = 1;

      update_elected_producers(ct);
      _gstate.last_producer_schedule_update = ct;
   }

#ifdef APOS_ENABLED
   void system_contract::initelects( uint32_t max_backup_producer_count ) {
      require_auth( get_self() );
      check(max_backup_producer_count >= min_backup_producer_count,
         "max_backup_producer_count must >= " + to_string(min_backup_producer_count));
      check(!_elect_gstate.is_init(), "elected producer has been initialized");

      _elect_gstate.elected_version = 1;
      _elect_gstate.max_backup_producer_count = max_backup_producer_count;

      auto elect_idx = _producers.get_index<"electedprod"_n>();
      eosio::proposed_producer_changes changes;

      CHECK(reinit_elected_producers(elect_idx, changes),
            "there must be at least " + to_string(_elect_gstate.min_producer_count() + 1) + " valid producers");

      auto ret = set_proposed_producers_ex( changes );
      CHECK(ret >= 0, "set proposed producers to native system failed(" + std::to_string(ret) + ")");

      _gstate.last_producer_schedule_update = current_block_time();
      _gstate.total_producer_vote_weight = 0; // clear the old vote info

      if( _gstate.thresh_activated_stake_time == time_point() ) {
         _gstate.thresh_activated_stake_time = current_time_point();
      }
   }

   template<typename elect_index_type>
   bool system_contract::reinit_elected_producers( const elect_index_type& elect_idx,
                                                   proposed_producer_changes& changes )
   {
      auto &main_changes = changes.main_changes;
      auto &backup_changes = changes.backup_changes;
      producer_elected_queue meq, beq;

      main_changes.clear_existed    = true;
      main_changes.producer_count   = _elect_gstate.max_main_producer_count;
      meq.last_producer_count       = _elect_gstate.max_main_producer_count;

      backup_changes.clear_existed  = true;
      backup_changes.producer_count = min_backup_producer_count;
      beq.last_producer_count       = min_backup_producer_count;

      // TODO: need using location to order producers?
      for( auto it = elect_idx.cbegin(); it != elect_idx.cend(); ++it ) {
         auto elected_info = it->get_elected_info();
         if (!it->active() || !is_prod_votes_valid(elected_info)) {
            break;
         }
         if (main_changes.changes.size() < main_changes.producer_count) {
            main_changes.changes.emplace(
               it->owner, eosio::producer_authority_add {
                  .authority = it->producer_authority
               }
            );
            if (!meq.tail.empty()) {
               meq.tail_prev = meq.tail;
            }
            meq.tail = elected_info;

            ASSERT(meq.tail_prev.empty() || meq.tail_prev > meq.tail);
         } else if (backup_changes.changes.size() < backup_changes.producer_count) {

            backup_changes.changes.emplace(
               it->owner, eosio::producer_authority_add {
                  .authority = it->producer_authority
               }
            );
            if (!beq.tail.empty()) {
               beq.tail_prev = beq.tail;
            }
            beq.tail = elected_info;

            if (meq.tail_next.empty()) {
               meq.tail_next = beq.tail;
            }
            ASSERT(beq.tail_prev.empty() || beq.tail_prev > beq.tail);
         } else { // backup_changes.changes.size() == min_backup_producer_count
            beq.tail_next = elected_info;
            break;
         }
      }

      if (beq.tail_next.empty()) {
         return false;
      }

      _elect_gstate.main_elected_queue = meq;
      _elect_gstate.backup_elected_queue = beq;
      return true;
   }

#endif//APOS_ENABLED

   void system_contract::register_producer(  const name& producer,
                                             const block_signing_authority& producer_authority,
                                             const string& url,
                                             uint16_t location,
                                             optional<uint32_t> reward_shared_ratio ) {

      if (reward_shared_ratio)
         CHECK(*reward_shared_ratio <= ratio_boost, "reward_shared_ratio is too large than " + to_string(ratio_boost));

      const auto& core_sym = core_symbol();
      auto prod = _producers.find( producer.value );
      const auto ct = current_time_point();

      eosio::public_key producer_key{};

      std::visit( [&](auto&& auth ) {
         if( auth.keys.size() == 1 ) {
            // if the producer_authority consists of a single key, use that key in the legacy producer_key field
            producer_key = auth.keys[0].key;
         }
      }, producer_authority );

      if (!amax_reward::is_producer_registered(reward_account, producer)) {
         amax_reward::regproducer_action reg_act{ reward_account, { {producer, active_permission} } };
         reg_act.send( producer );
      }

      if ( prod != _producers.end() ) {
         auto elect_idx = _producers.get_index<"electedprod"_n>();
         auto elected_info_old = prod->get_elected_info();
         _producers.modify( prod, producer, [&]( producer_info& info ){
            info.producer_key       = producer_key;
            info.is_active          = true;
            info.url                = url;
            info.location           = location;
            info.producer_authority = producer_authority;

            if (!info.ext) {
               info.ext = producer_info_ext{};
            }

            if (reward_shared_ratio)
               info.ext->reward_shared_ratio = *reward_shared_ratio;

            if ( info.last_claimed_time == time_point() )
               info.last_claimed_time = ct;

            if (elect_idx.iterator_to(info) == elect_idx.end()) {
               elect_idx.emplace_index(info, producer);
            }
         });
         #ifdef APOS_ENABLED
         if (_elect_gstate.is_init()) {
            ASSERT(prod->ext && elect_idx.iterator_to(*prod) != elect_idx.end());
            proposed_producer_changes changes;
            process_elected_producer(elected_info_old, prod->get_elected_info(), changes);

            save_producer_changes(changes, producer);
         }
         #endif//APOS_ENABLED
      } else {
         _producers.emplace( producer, [&]( producer_info& info ){
            info.owner              = producer;
            info.total_votes        = 0;
            info.producer_key       = producer_key;
            info.is_active          = true;
            info.url                = url;
            info.location           = location;
            info.last_claimed_time  = ct;
            info.unclaimed_rewards  = asset(0, core_sym);
            info.producer_authority = producer_authority;
            info.ext                = producer_info_ext{};
            if (reward_shared_ratio)
               info.ext->reward_shared_ratio = *reward_shared_ratio;
         });
      }

   }

   void system_contract::regproducer(  const name& producer, const eosio::public_key& producer_key,
                                       const string& url, uint16_t location,
                                       optional<uint32_t> reward_shared_ratio ) {
      // require_auth( producer );
      check( has_auth(producer) || has_auth(get_self()), "missing authority producer" );
      check( url.size() < 512, "url too long" );

      register_producer( producer, convert_to_block_signing_authority( producer_key ), url, location, reward_shared_ratio );
   }

   void system_contract::regproducer2( const name& producer,
                                       const block_signing_authority& producer_authority,
                                       const string& url,
                                       uint16_t location,
                                       optional<uint32_t> reward_shared_ratio) {
      // require_auth( producer );
      check( has_auth(producer) || has_auth(get_self()), "missing authority producer" );
      check( url.size() < 512, "url too long" );

      std::visit( [&](auto&& auth ) {
         check( auth.is_valid(), "invalid producer authority" );
      }, producer_authority );

      register_producer( producer, producer_authority, url, location, reward_shared_ratio );
   }


   void system_contract::addproducer( const name& producer,
                                       const block_signing_authority& producer_authority,
                                       const string& url,
                                       uint16_t location,
                                       optional<uint32_t> reward_shared_ratio) {
      require_auth( get_self() );
      check(is_account(producer), "producer account not found");
      regproducer2_action act{ get_self(), { {producer, active_permission} } };
      act.send( producer, producer_authority, url, location, reward_shared_ratio );
   }

   void system_contract::setvoteshare(   const name& producer, uint32_t reward_shared_ratio ) {

      CHECK( has_auth(producer) || has_auth(get_self()), "missing authority producer" );
      CHECK(reward_shared_ratio <= ratio_boost, "reward_shared_ratio is too large than " + to_string(ratio_boost))

      auto prod = _producers.get( producer.value, "producer not found" );
      CHECK(bool(prod.ext), "producer is not updated")
      _producers.modify( prod, producer, [&]( producer_info& p ){
         p.ext->reward_shared_ratio = reward_shared_ratio;
      });
   }


   void system_contract::unregprod( const name& producer ) {
      require_auth( producer );

      const auto& prod = _producers.get( producer.value, "producer not found" );
      _producers.modify( prod, same_payer, [&]( producer_info& info ){
         info.deactivate();
      });
   }

   bool system_contract::update_elected_producers( const block_timestamp& block_time ) {


      using value_type = std::pair<eosio::producer_authority, uint16_t>;

      auto fetch_top_producers = [&](std::vector< value_type > &top_producers, const auto& idx, const auto& is_valid) {

         for( auto it = idx.cbegin(); it != idx.cend() && top_producers.size() < 21 && it->active() && is_valid(*it); ++it ) {
            top_producers.emplace_back(
               eosio::producer_authority{
                  .producer_name = it->owner,
                  .authority     = it->producer_authority
               },
               it->location
            );
         }
      };

      std::vector< value_type > top_producers;
      top_producers.reserve(21);
      if(_elect_gstate.is_init()) {
         fetch_top_producers(top_producers, _producers.get_index<"electedprod"_n>(), [&](const auto& prod)->bool {
            return is_prod_votes_valid(prod.get_elected_votes());
         });
      } else {
         fetch_top_producers(top_producers, _producers.get_index<"prototalvote"_n>(), [](const auto& prod)->bool {
            return prod.total_votes > 0;
         });
      }

      if ( top_producers.size() == 0 || top_producers.size() < _gstate.last_producer_schedule_size ) {
         return false;
      }

      std::sort( top_producers.begin(), top_producers.end(), []( const value_type& lhs, const value_type& rhs ) {
         return lhs.first.producer_name < rhs.first.producer_name; // sort by producer name
         // return lhs.second < rhs.second; // sort by location
      } );

      std::vector<eosio::producer_authority> producers;

      producers.reserve(top_producers.size());
      for( auto& item : top_producers )
         producers.push_back( std::move(item.first) );

      if( set_proposed_producers( producers ) >= 0 ) {
         _gstate.last_producer_schedule_size = static_cast<decltype(_gstate.last_producer_schedule_size)>( top_producers.size() );
         return true;
      }

      return false;
   }

#ifdef APOS_ENABLED
   void system_contract::update_elected_producer_changes( const block_timestamp& block_time ) {

      static const uint32_t max_flush_elected_rows = 10;
      static const uint32_t min_flush_elected_changes = 300;
      static const uint32_t max_flush_elected_changes = 1000;

      proposed_producer_changes changes;
      // use empty changes to check that the native proposed producers is in a settable state
      if (eosio::set_proposed_producers(changes) < 0) {
         return;
      }

      uint32_t rows = 0;
      for (auto itr = _elected_changes.begin(); rows < max_flush_elected_rows && itr != _elected_changes.end(); ++rows, ++itr) {
         producer_change_helper::merge(itr->changes, changes);
         if (changes.main_changes.changes.size() + changes.backup_changes.changes.size() >= min_flush_elected_changes ) {
            break;
         }
      }
      if (eosio::set_proposed_producers(changes) > 0) {
         auto itr = _elected_changes.begin();
         for (size_t i = 0; i < rows && itr != _elected_changes.end(); ++i) {
            itr = _elected_changes.erase(itr);
         }
      }
   }
#endif//APOS_ENABLED

   double stake2vote( int64_t staked ) {
      /// TODO subtract 2080 brings the large numbers closer to this decade
      double weight = int64_t( (current_time_point().sec_since_epoch() - (block_timestamp::block_timestamp_epoch / 1000)) / (seconds_per_day * 7) )  / double( 52 );
      return double(staked) * std::pow( 2, weight );
   }

   void system_contract::voteproducer( const name& voter_name, const name& proxy, const std::vector<name>& producers ) {
      check(!bool(proxy), "proxy is unsupported");

      CHECK(!_elect_gstate.is_init(), "voteproducer is unsupported, use vote() action instead" );

      require_auth( voter_name );
      vote_stake_updater( voter_name );
      update_vote_weight_old( voter_name, proxy, producers, true );
      auto rex_itr = _rexbalance.find( voter_name.value );
      if( rex_itr != _rexbalance.end() && rex_itr->rex_balance.amount > 0 ) {
         check_voting_requirement( voter_name, "voter holding REX tokens must vote for at least 21 producers or for a proxy" );
      }
   }

   void system_contract::update_vote_weight_old( const name& voter_name, const name& proxy, const std::vector<name>& producers, bool voting ) {
      //validate input
      if ( proxy ) {
         check( producers.size() == 0, "cannot vote for producers and proxy at same time" );
         check( voter_name != proxy, "cannot proxy to self" );
      } else {
         check( producers.size() <= 30, "attempt to vote for too many producers" );
         for( size_t i = 1; i < producers.size(); ++i ) {
            check( producers[i-1] < producers[i], "producer votes must be unique and sorted" );
         }
      }

      auto voter = _voters.find( voter_name.value );
      check( voter != _voters.end(), "user must stake before they can vote" ); /// staking creates voter object
      check( !proxy || !voter->is_proxy, "account registered as a proxy is not allowed to use a proxy" );

      /**
       * The first time someone votes we calculate and set last_vote_weight. Since they cannot unstake until
       * after the chain has been activated, we can use last_vote_weight to determine that this is
       * their first vote and should consider their stake activated.
       */
      if( _gstate.thresh_activated_stake_time == time_point() && voter->last_vote_weight <= 0.0 ) {
         _gstate.total_activated_stake += voter->staked;
         if( _gstate.total_activated_stake >= min_activated_stake ) {
            _gstate.thresh_activated_stake_time = current_time_point();
         }
      }

      auto new_vote_weight = stake2vote( voter->staked );
      if( voter->is_proxy ) {
         new_vote_weight += voter->proxied_vote_weight;
      }

      struct producer_delta_t {
         double   vote_weight = 0.0;
         int64_t  elected_votes = 0;
         bool     is_new = false;
      };

      std::map<name, producer_delta_t> producer_deltas;
      if ( voter->last_vote_weight > 0 ) {
         if( voter->proxy ) {
            if (!proxy || proxy != voter->proxy ) {
               auto old_proxy = _voters.find( voter->proxy.value );
               check( old_proxy != _voters.end(), "old proxy not found" ); //data corruption
               _voters.modify( old_proxy, same_payer, [&]( auto& vp ) {
                     vp.proxied_vote_weight -= voter->last_vote_weight;
                     if (vp.votes.symbol != vote_symbol) {
                        vp.votes              = vote_asset_0;
                     }
                  });
               propagate_weight_change( *old_proxy, voter_name );
            }
         } else {
            for( const auto& p : voter->producers ) {
               auto& d = producer_deltas[p];
               d.vote_weight   -= voter->last_vote_weight;
               d.is_new = false;
            }
         }
      }

      if( proxy ) {
         auto new_proxy = _voters.find( proxy.value );
         check( new_proxy != _voters.end(), "invalid proxy specified" ); //if ( !voting ) { data corruption } else { wrong vote }
         check( !voting || new_proxy->is_proxy, "proxy not found" );
         double old_proxy_weight = 0;
         if (voter->proxy && proxy == voter->proxy) {
            old_proxy_weight     = voter->last_vote_weight;
         }

         if ( new_vote_weight >= 0 || old_proxy_weight > 0 ) {
            _voters.modify( new_proxy, same_payer, [&]( auto& vp ) {
                  vp.proxied_vote_weight     += new_vote_weight - old_proxy_weight;

                  if (vp.votes.symbol != vote_symbol) {
                     vp.votes                 = vote_asset_0;
                  }
               });
            propagate_weight_change( *new_proxy, voter_name );
         }
      } else {
         if( new_vote_weight >= 0 ) {
            for( const auto& p : producers ) {
               auto& d = producer_deltas[p];
               d.vote_weight += new_vote_weight;
               d.is_new = true;
            }
         }
      }

      auto elect_idx = _producers.get_index<"electedprod"_n>();
      for( const auto& pd : producer_deltas ) {
         auto pitr = _producers.find( pd.first.value );
         if( pitr != _producers.end() ) {
            if( voting && !pitr->active() && pd.second.is_new /* from new set */ ) {
               check( false, ( "producer " + pitr->owner.to_string() + " is not currently registered" ).data() );
            }
            _producers.modify( pitr, same_payer, [&]( auto& p ) {
               p.total_votes += pd.second.vote_weight;
               if ( p.total_votes < 0 ) { // floating point arithmetics can give small negative numbers
                  p.total_votes = 0;
               }
               _gstate.total_producer_vote_weight += pd.second.vote_weight;
            });

         } else {
            if( pd.second.is_new ) {
               check( false, ( "producer " + pd.first.to_string() + " is not registered" ).data() );
            }
         }
      }

      _voters.modify( voter, same_payer, [&]( auto& av ) {
         av.last_vote_weight = new_vote_weight;
         av.producers = producers;
         av.proxy     = proxy;

         if (av.votes.symbol != vote_symbol) {
            av.votes  = vote_asset_0;
         }
      });
   }

#ifdef APOS_ENABLED
   void system_contract::update_producer_elected_votes(  const std::vector<name>& producers,
                                                         const asset& votes_delta,
                                                         bool is_adding,
                                                         proposed_producer_changes& changes ) {
#else
   void system_contract::update_producer_elected_votes(  const std::vector<name>& producers,
                                                         const asset& votes_delta,
                                                         bool is_adding) {
#endif//APOS_ENABLED
      for( const auto& p : producers ) {
         auto pitr = _producers.find( p.value );

         CHECK( pitr != _producers.end(), "producer " + p.to_string() + " is not registered" );
         if (is_adding) {
            CHECK( pitr->active() , "producer " + pitr->owner.to_string() + " is not active" );
         }
         CHECK(pitr->ext, "producer " + pitr->owner.to_string() + " is not updated by regproducer")

         auto elected_info_old = pitr->get_elected_info();
         _producers.modify( pitr, same_payer, [&]( auto& p ) {
            p.total_votes = 0; // clear old vote info
            p.ext->elected_votes += votes_delta;
            CHECK( p.ext->elected_votes.amount >= 0, "producer's elected votes can not be negative" )
            _elect_gstate.total_producer_elected_votes += votes_delta.amount;
            check(_elect_gstate.total_producer_elected_votes >= 0, "total_producer_elected_votes can not be negative");
         });
#ifdef APOS_ENABLED
         process_elected_producer(elected_info_old, pitr->get_elected_info(), changes);
#endif//APOS_ENABLED
      }
   }

   void system_contract::addvote( const name& voter, const asset& votes ) {
      require_auth(voter);

      CHECK(votes.symbol == vote_symbol, "votes symbol mismatch")
      CHECK(votes.amount > 0, "votes must be positive")

      auto vote_staked = vote_to_core_asset(votes);
      token::transfer_action transfer_act{ token_account, { {voter, active_permission} } };
      transfer_act.send( voter, vote_account, vote_staked, "addvote" );

      auto now = current_time_point();
      auto voter_itr = _voters.find( voter.value );
      if( voter_itr != _voters.end() ) {
         if (voter_itr->producers.size() > 0) {
            #ifdef APOS_ENABLED
               proposed_producer_changes changes;
               update_producer_elected_votes(voter_itr->producers, votes, false, changes);
               save_producer_changes(changes, voter);
            #else
               update_producer_elected_votes(voter_itr->producers, votes, false);
            #endif//APOS_ENABLED
         }

         _voters.modify( voter_itr, same_payer, [&]( auto& v ) {
            if (v.votes.symbol != vote_symbol) {
               v.votes           = vote_asset_0;
            }
            v.votes             += votes;
         });
      } else {
         _voters.emplace( voter, [&]( auto& v ) {
            v.owner              = voter;
            v.votes              = votes;
         });
      }

      amax_reward::addvote_action addvote_act{ reward_account, { {get_self(), active_permission}, {voter, active_permission} } };
      addvote_act.send( voter, votes );
   }

   void system_contract::subvote( const name& voter, const asset& votes ) {
      require_auth(voter);

      CHECK(votes.symbol == vote_symbol, "votes symbol mismatch")
      CHECK(votes.amount > 0, "votes must be positive")

      auto voter_itr = _voters.find( voter.value );
      CHECK( voter_itr != _voters.end(), "voter not found" )

      CHECK( voter_itr->votes >= votes, "votes insufficent" )

      auto now = current_time_point();

      CHECKC( time_point(voter_itr->last_unvoted_time) + seconds(vote_interval_sec) < now, err::VOTE_ERROR, "Voter can only vote or subvote once a day" )

      vote_refund_table vote_refund_tbl( get_self(), voter.value );
      CHECKC( vote_refund_tbl.find( voter.value ) == vote_refund_tbl.end(), err::VOTE_REFUND_ERROR, "This account already has a vote refund" );

      auto votes_delta = -votes;
      #ifdef APOS_ENABLED
         proposed_producer_changes changes;
         update_producer_elected_votes(voter_itr->producers, votes_delta, false, changes);
         save_producer_changes(changes, voter);
      #else
         update_producer_elected_votes(voter_itr->producers, votes_delta, false);
      #endif//APOS_ENABLED

      _voters.modify( voter_itr, same_payer, [&]( auto& v ) {
         if (v.votes.symbol != vote_symbol) {
            v.votes           = vote_asset_0;
         }
         v.votes             += votes_delta;
         v.last_unvoted_time  = now;
      });

      vote_refund_tbl.emplace( voter, [&]( auto& r ) {
         r.owner = voter;
         r.votes = votes;
         r.request_time = now;
      });

      amax_reward::subvote_action subvote_act{ reward_account, { {get_self(), active_permission}, {voter, active_permission} } };
      subvote_act.send( voter, votes );

      eosio::transaction refund_trx;
      auto perms = permission_level{ voter, active_permission };
      refund_trx.actions.emplace_back( perms, _self, "refundvote"_n, voter );
      refund_trx.delay_sec = refund_delay_sec;
      eosio::cancel_deferred( voter.value ); // TODO: Remove this line when replacing deferred trxs is fixed
      refund_trx.send( voter.value, voter, true );

   }

   void system_contract::vote( const name& voter, const std::vector<name>& producers ) {
      require_auth(voter);

      check( producers.size() <= max_vote_producer_count, "attempt to vote for too many producers" );
      for( size_t i = 1; i < producers.size(); ++i ) {
         check( producers[i - 1] < producers[i], "producer votes must be unique and sorted" );
      }

      auto voter_itr = _voters.find( voter.value );
      check( voter_itr != _voters.end(), "voter not found" ); /// addvote creates voter object

      ASSERT( voter_itr->votes.amount >= 0 )
      CHECKC( voter_itr->producers != producers, err::VOTE_CHANGE_ERROR, "producers no change" )

      auto now = current_time_point();
      CHECK( time_point(voter_itr->last_unvoted_time) + seconds(vote_interval_sec) < now, "Voter can only vote or subvote once a day" )

      const auto& old_prods = voter_itr->producers;
      auto old_prod_itr = old_prods.begin();
      auto new_prod_itr = producers.begin();
      std::vector<name> removed_prods; removed_prods.reserve(old_prods.size());
      std::vector<name> modified_prods; removed_prods.reserve(old_prods.size());
      std::vector<name> added_prods;   added_prods.reserve(producers.size());
      while(old_prod_itr != old_prods.end() || new_prod_itr != producers.end()) {

         if (old_prod_itr != old_prods.end() && new_prod_itr != producers.end()) {
            if (old_prod_itr < new_prod_itr) {
               removed_prods.push_back(*old_prod_itr);
               old_prod_itr++;
            } else if (new_prod_itr < old_prod_itr) {
               added_prods.push_back(*new_prod_itr);
               new_prod_itr++;
            } else { // new_prod_itr == old_prod_itr
               modified_prods.push_back(*old_prod_itr);
               old_prod_itr++;
               new_prod_itr++;
            }
         } else if ( old_prod_itr != old_prods.end() ) { //  && new_prod_itr == producers.end()
               removed_prods.push_back(*old_prod_itr);
               old_prod_itr++;
         } else { // new_prod_itr != producers.end() && old_prod_itr == old_prods.end()
               added_prods.push_back(*new_prod_itr);
               new_prod_itr++;
         }
      }

      auto unvotes = -voter_itr->votes;
      #ifdef APOS_ENABLED
         proposed_producer_changes changes;
         update_producer_elected_votes(removed_prods, unvotes, false, changes);
         update_producer_elected_votes(modified_prods, vote_asset_0, false, changes);
         update_producer_elected_votes(added_prods, voter_itr->votes, false, changes);
         save_producer_changes(changes, voter);
      #else
         update_producer_elected_votes(removed_prods, unvotes, false);
         update_producer_elected_votes(modified_prods, vote_asset_0, false);
         update_producer_elected_votes(added_prods, voter_itr->votes, false);
      #endif//APOS_ENABLED

      amax_reward::voteproducer_action voteproducer_act{ reward_account, { {get_self(), active_permission}, {voter, active_permission} } };
      voteproducer_act.send( voter, producers );

      _voters.modify( voter_itr, same_payer, [&]( auto& v ) {
         if (v.votes.symbol != vote_symbol) {
            v.votes           = vote_asset_0;
         }
         v.producers          = producers;
         v.last_unvoted_time  = now;
      });
   }

   void system_contract::refundvote( const name& owner ) {
      vote_refund_table vote_refund_tbl( get_self(), owner.value );
      auto itr = vote_refund_tbl.find( owner.value );
      check( itr != vote_refund_tbl.end(), "no vote refund found" );
      check( itr->request_time + seconds(refund_delay_sec) <= current_time_point(), "refund period not mature yet" );

      auto vote_staked = vote_to_core_asset(itr->votes);
      token::transfer_action transfer_act{ token_account, { {vote_account, active_permission} } };
      transfer_act.send( vote_account, itr->owner, vote_staked, "refundvote" );
      vote_refund_tbl.erase( itr );
   }

   void system_contract::regproxy( const name& proxy, bool isproxy ) {
      check(false, "regproxy is unsupported");
      require_auth( proxy );

      auto pitr = _voters.find( proxy.value );
      if ( pitr != _voters.end() ) {
         check( isproxy != pitr->is_proxy, "action has no effect" );
         check( !isproxy || !pitr->proxy, "account that uses a proxy is not allowed to become a proxy" );
         _voters.modify( pitr, same_payer, [&]( auto& v ) {
               v.is_proxy = isproxy;
               if (v.votes.symbol != vote_symbol) {
                  v.votes           = vote_asset_0;
               }
            });
         propagate_weight_change( *pitr, proxy );
      } else {
         _voters.emplace( proxy, [&]( auto& p ) {
               p.owner  = proxy;
               p.is_proxy = isproxy;
            });
      }
   }

   void system_contract::propagate_weight_change( const voter_info& voter, const name& payer ) {
      check( !voter.proxy || !voter.is_proxy, "account registered as a proxy is not allowed to use a proxy" );
      double new_weight          = stake2vote( voter.staked );
      if ( voter.is_proxy ) {
         new_weight              += voter.proxied_vote_weight;
      }

      auto elect_idx = _producers.get_index<"electedprod"_n>();

      /// don't propagate small changes (1 ~= epsilon)
      if ( fabs( new_weight - voter.last_vote_weight ) > 1 )  {
         if ( voter.proxy ) {
            auto& proxy = _voters.get( voter.proxy.value, "proxy not found" ); //data corruption
            _voters.modify( proxy, same_payer, [&]( auto& pv ) {
                  pv.proxied_vote_weight     += new_weight - voter.last_vote_weight;
                  if (pv.votes.symbol != vote_symbol) {
                     pv.votes                 = vote_asset_0;
                  }
               }
            );
            propagate_weight_change( proxy, payer );
         } else {
            auto delta                       = new_weight - voter.last_vote_weight;
            const auto ct                    = current_time_point();
            for ( auto acnt : voter.producers ) {
               auto& prod = _producers.get( acnt.value, "producer not found" ); //data corruption
               _producers.modify( prod, same_payer, [&]( auto& p ) {
                  p.total_votes += delta;
                  if ( p.total_votes < 0 ) { // floating point arithmetics can give small negative numbers
                     p.total_votes = 0;
                  }
                  _gstate.total_producer_vote_weight += delta;
               });
            }
         }
      }
      _voters.modify( voter, same_payer, [&]( auto& v ) {
            v.last_vote_weight = new_weight;
            if (v.votes.symbol != vote_symbol) {
               v.votes         = vote_asset_0;
            }
         }
      );
   }

#ifdef APOS_ENABLED
   void system_contract::process_elected_producer(const producer_elected_info& prod_old,
                           const producer_elected_info& prod_new, proposed_producer_changes &changes) {

      if (!_elect_gstate.is_init()) {
         return;
      }

      auto &meq = _elect_gstate.main_elected_queue;
      auto &beq = _elect_gstate.backup_elected_queue;
      ASSERT(prod_old.name == prod_new.name);
      const auto& cur_name = prod_new.name;

      #ifdef TRACE_PRODUCER_CHANGES
      eosio::print("***** meq.last_producer_count=", meq.last_producer_count, "\n");
      eosio::print("beq.last_producer_count=", beq.last_producer_count, "\n");
      eosio::print("cur prod: ", cur_name, ", new_votes:", prod_new.elected_votes, ", old_votes:", prod_old.elected_votes,  "\n");
      eosio::print("meq tail_prev: ", meq.tail_prev.name, ",", meq.tail_prev.elected_votes, ",", "\n");
      eosio::print("meq tail: ", meq.tail.name, ",", meq.tail.elected_votes, ",", "\n");
      eosio::print("meq tail_next: ", meq.tail_next.name, ",", meq.tail_next.elected_votes, ",", "\n");
      eosio::print("beq tail_prev: ", beq.tail_prev.name, ",", beq.tail_prev.elected_votes, ",", "\n");
      eosio::print("beq tail: ", beq.tail.name, ",", beq.tail.elected_votes, ",", "\n");
      eosio::print("beq tail_next: ", beq.tail_next.name, ",", beq.tail_next.elected_votes, ",", "\n");
      #endif //TRACE_PRODUCER_CHANGES

      auto min_producer_count = _elect_gstate.max_main_producer_count + min_backup_producer_count + 1;
      ASSERT(meq.last_producer_count + beq.last_producer_count + 1 >= min_producer_count);
      ASSERT(!meq.tail.empty() && !meq.tail_prev.empty() && !beq.tail_next.empty() &&
             !beq.tail.empty() && !beq.tail_prev.empty() && !beq.tail_next.empty() && beq.tail_prev < meq.tail_next);

      auto &main_changes = changes.main_changes.changes;
      auto &backup_changes = changes.backup_changes.changes;

      bool refresh_main_tail_prev = false; // refresh by main_tail
      bool refresh_main_tail_next = false; // refresh by main_tail
      bool refresh_backup_tail_prev = false; // refresh by backup_tail
      bool refresh_backup_tail_next = false; // refresh by backup_tail

      // refresh queue positions
      auto idx = _producers.get_index<"electedprod"_n>();

      ASSERT(meq.last_producer_count > 0 && !meq.tail.empty());

      if (prod_old >= meq.tail) {
         bool modify_only = true;

         if (prod_new >= meq.tail_prev) {
            if (cur_name == meq.tail_prev.name) {
               meq.tail_prev.clear();
               refresh_main_tail_prev = true;
            } else if (cur_name == meq.tail.name) {
               meq.tail = meq.tail_prev;
               meq.tail_prev.clear();
               refresh_main_tail_prev = true;
            }

         } else if (prod_new >= meq.tail) { // and prod_new <= meq.tail_prev
            if (cur_name == meq.tail.name) {
               meq.tail = prod_new;
            } else if (cur_name == meq.tail_prev.name) {
               meq.tail_prev = prod_new;
            } else {
               meq.tail_prev.clear();
               refresh_main_tail_prev = true;
            }
         } else if (prod_new > meq.tail_next) { //prod_new < meq.tail
            if (cur_name != meq.tail.name) {
               meq.tail_prev = meq.tail;
            }
            meq.tail = prod_new;
         } else {// prod_new < meq.tail_next
            modify_only = false;
            // meq-, pop cur prod from main queue
            producer_change_helper::del(main_changes, prod_new);
            // beq-: del main tail next from backup queue
            producer_change_helper::del(backup_changes, meq.tail_next);
            // meq+: add main tail next to main queue
            producer_change_helper::add(main_changes, meq.tail_next);
            if (cur_name != meq.tail.name) {
               meq.tail_prev = meq.tail;
            }
            meq.tail = meq.tail_next;
            ASSERT(beq.last_producer_count > 0 && !beq.tail.empty())

            if (prod_new > beq.tail) {
               if (prod_new < beq.tail_prev) {
                  beq.tail_prev = prod_new;
               }
               // beq+: add cur prod to backup queue
               producer_change_helper::add(backup_changes, prod_new);
            } else if (prod_new > beq.tail_next) { // prod_new < beq.tail
               if (is_prod_votes_valid(prod_new) || beq.last_producer_count == min_backup_producer_count) {
                  // beq+: add cur prod to backup queue
                  producer_change_helper::add(backup_changes, prod_new);
                  beq.tail_prev = beq.tail;
                  beq.tail = prod_new;
               } else { // !is_prod_votes_valid(prod_new) && beq.last_producer_count > min_backup_producer_count
                  beq.tail_next = prod_new;
                  beq.last_producer_count--;
               }
            } else { // prod_new < beq.tail_next
               if(is_prod_votes_valid(beq.tail_next) || beq.last_producer_count == min_backup_producer_count) {
                  // beq+: add beq.tail_next to backup queue
                  producer_change_helper::add(backup_changes, beq.tail_next);
                  beq.tail_prev = beq.tail;
                  beq.tail = beq.tail_next;
                  beq.tail_next.clear();
                  refresh_backup_tail_next = true;
               } else { // !is_prod_votes_valid(beq.tail_next) && beq.last_producer_count > min_backup_producer_count
                  beq.last_producer_count--;
               }
            }
            meq.tail_next.clear();
            refresh_main_tail_next = true;
         }

         if (modify_only && prod_new.authority != prod_old.authority) {
            // meq*: modify cur prod in meq
            producer_change_helper::modify(main_changes, prod_new);
         }
      } else if (prod_new > meq.tail) { // prod_old < meq.tail

         // meq-: del meq.tail from main queue
         producer_change_helper::del(main_changes, meq.tail);
         // meq+: add cur prod to main queue
         producer_change_helper::add(main_changes, prod_new);
         // beq+: add meq.tail to backup queue
         producer_change_helper::add(backup_changes, meq.tail);

         meq.tail_next = meq.tail;
         if (prod_new > meq.tail_prev) {
            meq.tail = meq.tail_prev;
            meq.tail_prev.clear();
            refresh_main_tail_prev = true;
         } else { // prod_new < meq.tail_prev && prod_new > meq.tail
            meq.tail = prod_new;
         }

         if (prod_old >= beq.tail) {
            // beq-: del cur prod from backup queue
            producer_change_helper::del(backup_changes, prod_new);
            if (prod_old == beq.tail_prev) {
                  beq.tail_prev.clear();
                  refresh_backup_tail_prev = true;
            } else if (prod_old == beq.tail) {
               beq.tail = beq.tail_prev;
               beq.tail_prev.clear();
               refresh_backup_tail_prev = true;
            }
         } else { // prod_old < beq.tail
            bool is_pop_tail = false;
            if (beq.last_producer_count == _elect_gstate.max_backup_producer_count) {
               is_pop_tail = true;
            } else { // beq.last_producer_count < ext.max_backup_producer_count
               if (prod_old == beq.tail_next) {
                  queue_helper::fetch_next(idx, beq.tail, beq.tail_next, false, "backup queue tail next");
                  is_pop_tail = beq.tail_next.empty();
               }
            }

            if (is_pop_tail) {
               // beq-: pop backup tail from backup queue
               producer_change_helper::del(backup_changes, beq.tail);
               beq.tail_next = beq.tail;
               beq.tail = beq.tail_prev;
               beq.tail_prev.clear();
               refresh_backup_tail_prev = true;
            } else {
               beq.last_producer_count++;
            }
         }
      } else if (prod_old >= beq.tail) { // && prod_old < meq.tail && prod_new < meq.tail
         bool modify_only = true;

         if (prod_new >= meq.tail_next) {
            if (prod_old == beq.tail_prev) {
               refresh_backup_tail_prev = true;
            } else if (prod_old == beq.tail) {
               beq.tail = beq.tail_prev;
               refresh_backup_tail_prev = true;
            }
            meq.tail_next = prod_new;
         } else if (prod_new >= beq.tail_prev) { // && prod_new < meq.tail_next
            if (prod_old == meq.tail_next) {
               refresh_main_tail_next = true;
            } else if (prod_old == beq.tail_prev) {
               if (prod_new != beq.tail_prev) {
                  refresh_backup_tail_prev = true;
               }
            } else if (prod_old == beq.tail) {
               beq.tail = beq.tail_prev;
               refresh_backup_tail_prev = true;
            }
         } else if (prod_new >= beq.tail) { // && prod_new < beq.tail_prev
            if (prod_old == beq.tail) {
               beq.tail = prod_new;
            } else { // prod_old != beq.tail {
               beq.tail_prev = prod_new;
               if (prod_old == meq.tail_next) {
                  meq.tail_next.clear();
                  refresh_main_tail_next = true;
               }
            }

         } else if ( prod_new > beq.tail_next ) { // && prod_new < beq.tail
            if (is_prod_votes_valid(prod_new) || beq.last_producer_count == min_backup_producer_count) {
               if (prod_old != beq.tail) {
                  beq.tail_prev = beq.tail;
                  if (prod_old == meq.tail_next) {
                     refresh_main_tail_next = true;
                  }
               }
               beq.tail = prod_new;
            } else { // !is_prod_votes_valid(prod_new) && beq.last_producer_count > min_backup_producer_count
               if (prod_old == beq.tail) {
                  beq.tail = beq.tail_prev;
                  refresh_backup_tail_prev = true;
               } else if (prod_old == beq.tail_prev) {
                  refresh_backup_tail_prev = true;
               } else if (prod_old == meq.tail_next) {
                  refresh_main_tail_next = true;
               }

               beq.tail_next = prod_new;
               beq.last_producer_count--;
               // beq-: del cur prod from backup queue
               producer_change_helper::del(backup_changes, prod_new);
               modify_only = false;
            }
         } else { // prod_new < beq.tail_next
            modify_only = false;
            if (is_prod_votes_valid(beq.tail_next) || beq.last_producer_count == min_backup_producer_count) {
               if (prod_old != beq.tail) {
                  beq.tail_prev = beq.tail;
                  beq.tail = beq.tail_next;
               }
               beq.tail = beq.tail_next;
               refresh_backup_tail_next = true;
               if (prod_old == meq.tail_next) {
                  meq.tail_next.clear();
                  refresh_main_tail_next = true;
               }
               // beq-: del cur prod from backup queue
               producer_change_helper::del(backup_changes, prod_new);
               // beq+: add beq.tail_next to backup queue
               producer_change_helper::add(backup_changes, beq.tail_next);
            } else { // !is_prod_votes_valid(beq.tail_next) && beq.last_producer_count > min_backup_producer_count
               if (prod_old == beq.tail) {
                  beq.tail = beq.tail_prev;
                  refresh_backup_tail_prev = true;
               } else if (prod_old == beq.tail_prev) {
                  refresh_backup_tail_prev = true;
               } else if (prod_old == meq.tail_next) {
                  refresh_main_tail_next = true;
               }

               beq.last_producer_count--;
               // beq-: del cur prod from backup queue
               producer_change_helper::del(backup_changes, prod_new);
            }
         }
         if (modify_only && prod_new.authority != prod_old.authority) {
            // meq*: modify cur prod in beq
            producer_change_helper::modify(backup_changes, prod_new);
         }
      } else { // prod_old < beq.tail && prod_new < meq.tail

         if (prod_new >= beq.tail) {
            ASSERT(prod_new != beq.tail)
            // beq+: add cur prod to backup queue
            producer_change_helper::add(backup_changes, prod_new);

            if ( beq.last_producer_count < _elect_gstate.max_backup_producer_count &&
                 prod_old != beq.tail_next &&
                 is_prod_votes_valid(beq.tail) )
            {
               beq.last_producer_count++;
               if (prod_new < beq.tail_prev) {
                  beq.tail_prev = prod_new;
               } else if (prod_new > meq.tail_next) { // prod_new > beq.tail_prev
                  meq.tail_next = prod_new;
               }
               if (prod_old == beq.tail_next) {
                  refresh_backup_tail_next = true;
               }
            } else {
               // beq-: pop beq.tail from backup queue
               producer_change_helper::del(backup_changes, beq.tail);
               beq.tail_next = beq.tail;
               if (prod_new < beq.tail_prev) {
                  beq.tail = prod_new;
               } else { // prod_new > beq.tail_prev
                  beq.tail = beq.tail_prev;
                  refresh_backup_tail_prev = true;

                  if (prod_new > meq.tail_next) { // prod_new > beq.tail_prev
                     meq.tail_next = prod_new;
                  }
               }
            }

         } else if (prod_new >= beq.tail_next) { // prod_new < beq.tail
            if (  beq.last_producer_count < _elect_gstate.max_backup_producer_count &&
                  prod_old != beq.tail_next &&
                  is_prod_votes_valid(prod_new) )
            {
               // beq+: add cur prod to backup queue
               producer_change_helper::add(backup_changes, prod_new);
               beq.last_producer_count++;
               beq.tail_prev = beq.tail;
               beq.tail = prod_new;
            } else { // beq.last_producer_count == ext.max_backup_producer_count || prod_old == beq.tail_next || is_prod_votes_valid(prod_new)
               beq.tail_next = prod_new;
            }
         } else { // prod_new < beq.tail_next
            if ( beq.last_producer_count < _elect_gstate.max_backup_producer_count &&
               prod_old != beq.tail_next &&
               is_prod_votes_valid(beq.tail_next) )
            {
               // beq+: add beq.tail_next to backup queue
               producer_change_helper::add(backup_changes, beq.tail_next);
               beq.last_producer_count++;
               beq.tail_prev = beq.tail;
               beq.tail = beq.tail_next;
               refresh_backup_tail_next = true;
            } else if (prod_old == beq.tail_next) {
               refresh_backup_tail_next = true;
            }
         }
      }

      if (refresh_main_tail_prev) {
         queue_helper::fetch_prev(idx, meq.tail, meq.tail_prev, true, "main queue tail prev");
      }

      if (refresh_main_tail_next) {
         queue_helper::fetch_next(idx, meq.tail, meq.tail_next, true, "main queue tail next");
      }

      if (refresh_backup_tail_prev) {
         queue_helper::fetch_prev(idx, beq.tail, beq.tail_prev, true, "backup queue tail prev");
      }

      if (refresh_backup_tail_next) {
         queue_helper::fetch_next(idx, beq.tail, beq.tail_next, true, "backup queue tail next");
      }

      changes.main_changes.producer_count = meq.last_producer_count;
      changes.backup_changes.producer_count = beq.last_producer_count;

   }

   void system_contract::save_producer_changes(proposed_producer_changes &changes, const name& payer ) {
      if (!_elect_gstate.is_init()) {
         return;
      }

      if (_elect_gstate.producer_change_interrupted) {
         if (  _elect_gstate.backup_elected_queue.last_producer_count >= min_backup_producer_count &&
               is_prod_votes_valid(_elect_gstate.backup_elected_queue.tail)) {
            auto elect_idx = _producers.get_index<"electedprod"_n>();
            eosio::proposed_producer_changes init_changes;
            if (reinit_elected_producers(elect_idx, init_changes)) {
               _elected_changes.emplace( payer, [&]( auto& c ) {
                     _elect_gstate.last_producer_change_id++;
                     c.id           = _elect_gstate.last_producer_change_id;
                     c.changes      = init_changes;
                     c.created_at   = eosio::current_time_point();
               });
               _elect_gstate.producer_change_interrupted = false;
            }
         }
         return;
      }

      if (  _elect_gstate.backup_elected_queue.last_producer_count <= min_backup_producer_count &&
            !is_prod_votes_valid(_elect_gstate.backup_elected_queue.tail)) {
         _elect_gstate.producer_change_interrupted = true;
         return;
      }

      if ( !changes.backup_changes.changes.empty() || !changes.main_changes.changes.empty() ) {
         _elected_changes.emplace( payer, [&]( auto& c ) {
               _elect_gstate.last_producer_change_id++;
               c.id           = _elect_gstate.last_producer_change_id;
               c.changes      = changes;
               c.created_at   = eosio::current_time_point();
         });
      }

   }

#endif//APOS_ENABLED

   inline asset system_contract::vote_to_core_asset(const asset& votes) {
      int128_t amount = votes.amount * vote_to_core_asset_factor;
      CHECK( amount >= 0 && amount <= std::numeric_limits<int64_t>::max(), "votes out of range")
      return asset((int64_t)amount, core_symbol());
   }
} /// namespace eosiosystem
