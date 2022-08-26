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

#include <type_traits>
#include <limits>
#include <set>
#include <algorithm>
#include <cmath>

namespace eosiosystem {

   using eosio::const_mem_fun;
   using eosio::current_time_point;
   using eosio::current_block_time;
   using eosio::indexed_by;
   using eosio::microseconds;
   using eosio::singleton;

   static constexpr uint32_t main_producer_count = 21;
   static constexpr uint32_t backup_producer_count = 10000;

   void system_contract::initelects( const name& payer ) {
      require_auth( payer );
      bool is_init = false;
      if (!is_init) {
         is_init = true;
         auto block_time = current_block_time();

         _gstate.last_producer_schedule_update = block_time;
         eosio::proposed_producer_changes changes;
         changes.main_changes.clear_existed = true;
         changes.backup_changes.clear_existed = true;

         auto &main_changes = changes.main_changes;
         auto idx = _producers.get_index<"prototalvote"_n>();
         amax_global_state_ext ext;

         // TODO: need using location to order producers?
         for( auto it = idx.cbegin(); it != idx.cend() && main_changes.changes.size() < main_producer_count - 1 && 0 < it->total_votes && it->active(); ++it ) {
            main_changes.changes.emplace(
               it->owner, eosio::producer_authority_add {
                  .authority = it->producer_authority
               }
            );
            idx.modify( it, payer, [&]( auto& p ) {
               p.ext = producer_info_ext{
                  .elected_votes = p.total_votes;
               };
            });
            // ext.main_producer_tail = {it->owner, it->total_votes};
         }
         main_changes.producer_count = main_changes.changes.size();

         eosio::check(main_changes.producer_count > 0, "top main producer count is 0");

         // if( top_producers.size() == 0 || top_producers.size() < _gstate.last_producer_schedule_size ) {
         //    return;
         // }

         auto ret = set_proposed_producers_ex( changes );
         CHECK(ret >= 0, "set proposed producers to native system failed(" + std::to_string(ret) + ")");

         _gstate.last_producer_schedule_size = main_changes.producer_count;
         _gstate.ext = ext;
         // clear changes

      }
   }

   void system_contract::register_producer( const name& producer, const eosio::block_signing_authority& producer_authority, const std::string& url, uint16_t location ) {

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

      if ( prod != _producers.end() ) {
         _producers.modify( prod, producer, [&]( producer_info& info ){
            info.producer_key       = producer_key;
            info.is_active          = true;
            info.url                = url;
            info.location           = location;
            info.producer_authority = producer_authority;
            if ( info.last_claimed_time == time_point() )
               info.last_claimed_time = ct;
         });
      } else {
         _producers.emplace( producer, [&]( producer_info& info ){
            info.owner              = producer;
            info.total_votes        = 0;
            info.producer_key       = producer_key;
            info.is_active          = true;
            info.url                = url;
            info.location           = location;
            info.last_claimed_time    = ct;
            info.unclaimed_rewards     = asset(0, core_sym);
            info.producer_authority = producer_authority;
         });
      }

   }

   void system_contract::regproducer( const name& producer, const eosio::public_key& producer_key, const std::string& url, uint16_t location ) {
      require_auth( producer );
      check( url.size() < 512, "url too long" );

      register_producer( producer, convert_to_block_signing_authority( producer_key ), url, location );
   }

   void system_contract::regproducer2( const name& producer, const eosio::block_signing_authority& producer_authority, const std::string& url, uint16_t location ) {
      require_auth( producer );
      check( url.size() < 512, "url too long" );

      std::visit( [&](auto&& auth ) {
         check( auth.is_valid(), "invalid producer authority" );
      }, producer_authority );

      register_producer( producer, producer_authority, url, location );
   }

   void system_contract::unregprod( const name& producer ) {
      require_auth( producer );

      const auto& prod = _producers.get( producer.value, "producer not found" );
      _producers.modify( prod, same_payer, [&]( producer_info& info ){
         info.deactivate();
      });
   }

   void system_contract::update_elected_producers( const block_timestamp& block_time ) {
      _gstate.last_producer_schedule_update = block_time;

      auto idx = _producers.get_index<"prototalvote"_n>();

      using value_type = std::pair<eosio::producer_authority, uint16_t>;
      std::vector< value_type > top_producers;
      top_producers.reserve(21);

      for( auto it = idx.cbegin(); it != idx.cend() && top_producers.size() < 21 && 0 < it->total_votes && it->active(); ++it ) {
         top_producers.emplace_back(
            eosio::producer_authority{
               .producer_name = it->owner,
               .authority     = it->producer_authority
            },
            it->location
         );
      }

      if( top_producers.size() == 0 || top_producers.size() < _gstate.last_producer_schedule_size ) {
         return;
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
      }
   }

   double stake2vote( int64_t staked ) {
      /// TODO subtract 2080 brings the large numbers closer to this decade
      double weight = int64_t( (current_time_point().sec_since_epoch() - (block_timestamp::block_timestamp_epoch / 1000)) / (seconds_per_day * 7) )  / double( 52 );
      return double(staked) * std::pow( 2, weight );
   }

   void system_contract::voteproducer( const name& voter_name, const name& proxy, const std::vector<name>& producers ) {
      require_auth( voter_name );
      vote_stake_updater( voter_name );
      update_votes( voter_name, proxy, producers, true );
      auto rex_itr = _rexbalance.find( voter_name.value );
      if( rex_itr != _rexbalance.end() && rex_itr->rex_balance.amount > 0 ) {
         check_voting_requirement( voter_name, "voter holding REX tokens must vote for at least 21 producers or for a proxy" );
      }
   }

   void system_contract::update_votes( const name& voter_name, const name& proxy, const std::vector<name>& producers, bool voting ) {
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

      std::map<name, std::pair<double, bool /*new*/> > producer_deltas;
      if ( voter->last_vote_weight > 0 ) {
         if( voter->proxy ) {
            auto old_proxy = _voters.find( voter->proxy.value );
            check( old_proxy != _voters.end(), "old proxy not found" ); //data corruption
            _voters.modify( old_proxy, same_payer, [&]( auto& vp ) {
                  vp.proxied_vote_weight -= voter->last_vote_weight;
               });
            propagate_weight_change( *old_proxy );
         } else {
            for( const auto& p : voter->producers ) {
               auto& d = producer_deltas[p];
               d.first -= voter->last_vote_weight;
               d.second = false;
            }
         }
      }

      if( proxy ) {
         auto new_proxy = _voters.find( proxy.value );
         check( new_proxy != _voters.end(), "invalid proxy specified" ); //if ( !voting ) { data corruption } else { wrong vote }
         check( !voting || new_proxy->is_proxy, "proxy not found" );
         if ( new_vote_weight >= 0 ) {
            _voters.modify( new_proxy, same_payer, [&]( auto& vp ) {
                  vp.proxied_vote_weight += new_vote_weight;
               });
            propagate_weight_change( *new_proxy );
         }
      } else {
         if( new_vote_weight >= 0 ) {
            for( const auto& p : producers ) {
               auto& d = producer_deltas[p];
               d.first += new_vote_weight;
               d.second = true;
            }
         }
      }

      const auto ct = current_time_point();
      double delta_change_rate         = 0.0;
      double total_inactive_vpay_share = 0.0;
      for( const auto& pd : producer_deltas ) {
         auto pitr = _producers.find( pd.first.value );
         if( pitr != _producers.end() ) {
            if( voting && !pitr->active() && pd.second.second /* from new set */ ) {
               check( false, ( "producer " + pitr->owner.to_string() + " is not currently registered" ).data() );
            }
            double init_total_votes = pitr->total_votes;
            _producers.modify( pitr, same_payer, [&]( auto& p ) {
               p.total_votes += pd.second.first;
               if ( p.total_votes < 0 ) { // floating point arithmetics can give small negative numbers
                  p.total_votes = 0;
               }
               _gstate.total_producer_vote_weight += pd.second.first;
               //check( p.total_votes >= 0, "something bad happened" );
            });
         } else {
            if( pd.second.second ) {
               check( false, ( "producer " + pd.first.to_string() + " is not registered" ).data() );
            }
         }
      }

      _voters.modify( voter, same_payer, [&]( auto& av ) {
         av.last_vote_weight = new_vote_weight;
         av.producers = producers;
         av.proxy     = proxy;
      });
   }

   void system_contract::regproxy( const name& proxy, bool isproxy ) {
      require_auth( proxy );

      auto pitr = _voters.find( proxy.value );
      if ( pitr != _voters.end() ) {
         check( isproxy != pitr->is_proxy, "action has no effect" );
         check( !isproxy || !pitr->proxy, "account that uses a proxy is not allowed to become a proxy" );
         _voters.modify( pitr, same_payer, [&]( auto& p ) {
               p.is_proxy = isproxy;
            });
         propagate_weight_change( *pitr );
      } else {
         _voters.emplace( proxy, [&]( auto& p ) {
               p.owner  = proxy;
               p.is_proxy = isproxy;
            });
      }
   }

   void system_contract::propagate_weight_change( const voter_info& voter ) {
      check( !voter.proxy || !voter.is_proxy, "account registered as a proxy is not allowed to use a proxy" );
      double new_weight = stake2vote( voter.staked );
      if ( voter.is_proxy ) {
         new_weight += voter.proxied_vote_weight;
      }

      /// don't propagate small changes (1 ~= epsilon)
      if ( fabs( new_weight - voter.last_vote_weight ) > 1 )  {
         if ( voter.proxy ) {
            auto& proxy = _voters.get( voter.proxy.value, "proxy not found" ); //data corruption
            _voters.modify( proxy, same_payer, [&]( auto& p ) {
                  p.proxied_vote_weight += new_weight - voter.last_vote_weight;
               }
            );
            propagate_weight_change( proxy );
         } else {
            auto delta = new_weight - voter.last_vote_weight;
            const auto ct = current_time_point();
            double delta_change_rate         = 0;
            double total_inactive_vpay_share = 0;
            for ( auto acnt : voter.producers ) {
               auto& prod = _producers.get( acnt.value, "producer not found" ); //data corruption
               const double init_total_votes = prod.total_votes;
               _producers.modify( prod, same_payer, [&]( auto& p ) {
                  p.total_votes += delta;
                  _gstate.total_producer_vote_weight += delta;
               });
            }

         }
      }
      _voters.modify( voter, same_payer, [&]( auto& v ) {
            v.last_vote_weight = new_weight;
         }
      );
   }


   void system_contract::process_elected_producer(const name& producer_name, const double& delta_votes, producer_info &prod_info) {

      if (!_gstate.ext.has_value())
         return;

      auto &meq = _gstate.ext->main_elected_queue;
      auto &beq = _gstate.ext->backup_elected_queue;
      if (meq.last_producer_count == 0)
         return;


      producer_elected_votes cur_old_prod = {prod_info.owner, prod_info.total_votes};
      producer_elected_votes cur_new_prod = {prod_info.owner, std::max(0.0, prod_info.total_votes + delta_votes) };
      const auto& cur_name = prod_info.owner;

      bool refresh_main_tail_prev = false; // refresh by main_tail
      bool refresh_backup_tail_prev = false; // refresh by backup_tail

      if (meq.last_producer_count == 0) { // main queue is empty
         // TODO: need to process empty main queue??
         ASSERT(meq.tail.empty() && meq.tail_prev.empty() && meq.tail_next.empty())
         // TODO: add cur prod to main queue
         meq.tail = cur_new_prod;
         meq.last_producer_count++;
         return;
      }

      // if: (meq.last_producer_count > 0)
      ASSERT(!meq.tail.empty()) // main queue is not empty
      ASSERT(main_producer_count > 1)
      if (meq.last_producer_count == 1) { // main queue is empty
         ASSERT(meq.tail.empty())
         if (cur_name == meq.tail.name) {
            meq.tail = cur_new_prod;
            // Can not substract main bp count.
            // So, cur prod can not be deleted, even if its elected votes is 0
         } else {
            ASSERT(cur_old_prod < meq.tail)
            if (cur_new_prod.elected_votes > 0) {
               if (cur_new_prod > meq.tail) {
                  meq.tail_prev = cur_new_prod;
               } else {
                  meq.tail_prev = meq.tail;
                  meq.tail = cur_new_prod;
               }

               // TODO: add cur prod to main queue
               meq.last_producer_count++;
            }
         }
         return;
      }

      // if: (meq.last_producer_count > 1)
      ASSERT(!meq.tail_prev.empty())

      // ASSERT(!beq.tail.empty());
      if (_gstate.last_producer_schedule_size < main_producer_count) { // main queue is not full
         // Can not substract main bp count.
         // So, cur prod can not be deleted, even if its elected votes is 0
         if ( cur_name == meq.tail.name ) { // cur prod was main tail
            if (cur_new_prod > meq.tail_prev) {
               meq.tail = meq.tail_prev;
               meq.tail_prev.clear();
               refresh_main_tail_prev = true;
            } // else no change

         } else if (cur_old_prod > meq.tail) {
            // cur prod was main producer and not main producer tail

            if (cur_new_prod < meq.tail) {
               meq.tail_prev = meq.tail;
               meq.tail = cur_new_prod;
               // meq.tail_next no change
            } else { // cur_new_prod > meq.tail
               if ( cur_name == meq.tail_prev.name) { // cur prod was main tail prev
                  if (cur_new_prod.elected_votes > cur_old_prod.elected_votes) {
                     meq.tail_prev.clear();
                     refresh_main_tail_prev = true;
                  }
               } else {
                  if (cur_new_prod < meq.tail_prev) {
                     meq.tail_prev = cur_new_prod;
                  }
               }
            }
         } else {
            // cur prod was not main producer
            if (cur_new_prod.elected_votes > 0) {
               // TODO: add cur prod to main queue
               meq.last_producer_count++;
               if (cur_new_prod < meq.tail) {
                  meq.tail_prev = meq.tail;
                  meq.tail = cur_new_prod;
               } else if (cur_new_prod < meq.tail_prev) {
                  meq.tail_prev = cur_new_prod;
               }
            }

            // TODO: add main producer change
         }
         meq.tail_next.clear(); // next should be empty
         // TODO: refresh_elected_queue_info()
         return;
      }

      // if: (_gstate.last_producer_schedule_size == main_producer_count) { // main queue is full

      if (cur_old_prod > meq.tail || cur_name == meq.tail.name) {
         // producer was main producer and not main producer tail

         if (cur_new_prod > meq.tail_prev) {
            if (cur_name == meq.tail_prev.name) {
               meq.tail_prev.clear();
               refresh_main_tail_prev = true;
            } else if (cur_name == meq.tail.name) {
               meq.tail = meq.tail_prev;
               meq.tail_prev.clear();
               refresh_main_tail_prev = true;
            } // else no change

         } else if (cur_new_prod > meq.tail) {
            if (cur_name != meq.tail_prev.name && cur_name != meq.tail.name) {
               meq.tail_prev.clear();
               refresh_main_tail_prev = true;
            }
         } else { //cur_new_prod < meq.tail
            if (meq.tail_next.empty() || cur_new_prod > meq.tail_next) {
               if (meq.tail_next.empty()) {
                  ASSERT(beq.last_producer_count == 0)
                  ASSERT(beq.tail.empty())
               }
               if (cur_name != meq.tail.name) {
                  meq.tail_prev = meq.tail;
                  meq.tail = cur_new_prod;
               }
            } else {// !meq.tail_next.empty() && cur_new_prod < meq.tail_next
               // TODO: pop cur pord from main queue
               // TODO: push main tail next to main queue
               if (cur_name != meq.tail.name) {
                  meq.tail_prev = meq.tail;
               }
               meq.tail = meq.tail_next;
               // TODO: need to push cur prod into backup queue??
            }
         }



         if (cur_name == meq.tail_prev.name) {
            cur_old_prod = meq.tail;

         }
         if ( !meq.tail_next.empty() && (cur_new_prod < meq.tail_next) ) {
            // TODO: cur prod change from main producer to others
         } else if (cur_new_prod < meq.tail) {
            meq.tail_prev = meq.tail;
            meq.tail = cur_new_prod;
            // meq.tail_next no change
         } else { // cur_new_prod > meq.tail
            if (!meq.tail_prev.empty()) {
               if ( cur_new_prod.name == meq.tail_prev.name) {
                  if (cur_new_prod.elected_votes > cur_old_prod.elected_votes) {
                     meq.tail_prev.clear();
                     refresh_main_tail_prev = true;
                  }
               } else {
                  if (cur_new_prod < meq.tail_prev) {
                     meq.tail_prev = cur_new_prod;
                  }
               }
            } else {
               meq.tail_prev = cur_new_prod;
            }
         }
         return;
      }

      // main queue is full, and
      // if (cur_old_prod < meq.tail)

      if (cur_new_prod > meq.tail) {
         // TODO: pop main tail from main queue
         auto old_main_tail = meq.tail;
         if (cur_new_prod < meq.tail_prev) {
            meq.tail = cur_new_prod;
         } else { // cur_new_prod > meq.tail_prev
            meq.tail = meq.tail_prev;
         }

         // TODO: push old main tail into backup queue

      }

      // process backup queue








         // TODO: add main producer change
         _gstate.last_producer_schedule_size++;
         if (!meq.tail_prev.empty()) {
            ASSERT(cur_new_prod.name != meq.tail_prev.name);
            if (cur_new_prod < meq.tail_prev) {
               meq.tail_prev = cur_new_prod;
            }
         } else {
            meq.tail_prev = cur_new_prod;
         }
      } else {
         ASSERT(main_producer_count > 1 && !meq.tail_prev.empty());
         ASSERT(cur_new_prod.name != meq.tail_prev.name);
         auto old_main_producer_tail = meq.tail;
         if (cur_new_prod < meq.tail_prev) {
            meq.tail = cur_new_prod;
         } else {
            meq.tail = meq.tail_prev;
            meq.tail_prev.clear();
            refresh_main_tail_prev = true;
         }

         // TODO: add old_main_producer_tail to backup_producers
         meq.tail_next = old_main_producer_tail;
         // _gstate.ext->backup_producer_head = old_main_producer_tail;

         if (beq.last_producer_count > 0) {

            if (beq.last_producer_count == 1 && cur_name == beq.tail.name) {
               ASSERT(beq.tail_prev.empty());
               beq.tail = old_main_producer_tail;
               // TODO: del curl prod from backup_producers;
            } else if (cur_name == beq.tail.name) {
               beq.tail = beq.tail_prev;
               beq.tail_prev.clear();
               refresh_backup_tail_prev = true;
               // TODO: del curl prod from backup_producers;
            } else if (cur_old_prod > beq.tail) {
               ASSERT(!beq.tail_prev.empty());
               if (beq.last_producer_count == 2) {
                  beq.tail_prev = old_main_producer_tail;
               } else {
                  beq.tail_prev.clear();
                  refresh_backup_tail_prev = true;
               }
               // TODO: del curl prod from backup_producers;
            } else {
               ASSERT(backup_producer_count > 1);
               if (beq.last_producer_count == 1) {
                  ASSERT(beq.tail_prev.empty());
                  beq.tail = old_main_producer_tail;
                  beq.last_producer_count++;
               } else if (beq.last_producer_count < backup_producer_count) {
                  ASSERT(!beq.tail_prev.empty());
                  beq.last_producer_count++;
               } else {
                  ASSERT(!beq.tail_prev.empty());
                  // pop backup tail
                  // TODO: del backup tail
                  beq.tail_next = beq.tail;
                  beq.tail = beq.tail_prev;
                  beq.tail_prev.clear();
                  refresh_backup_tail_prev = true;
               }
            }
         } else {
            meq.tail_next = old_main_producer_tail;
            // _gstate.ext->backup_producer_head = old_main_producer_tail;
            beq.tail = old_main_producer_tail;
            beq.last_producer_count++;
         }

      }






      if ( prod_info.owner == meq.tail.name ) {
         // producer was main producer tail
         if (!meq.tail_prev.empty() && cur_new_prod > meq.tail_prev) {
            meq.tail = meq.tail_prev;
            meq.tail_prev.clear();
            refresh_main_tail_prev = true;
            // meq.tail_next no change
         } else if ( !meq.tail_next.empty() && (cur_new_prod < meq.tail_next) ) {
               // TODO: cur prod change from main producer to others

         } // else no change

      } else if (cur_old_prod > meq.tail) {
         // producer was main producer and not main producer tail
         if ( !meq.tail_next.empty() && (cur_new_prod < meq.tail_next) ) {
            // TODO: cur prod change from main producer to others
         } else if (cur_new_prod < meq.tail) {
            meq.tail_prev = meq.tail;
            meq.tail = cur_new_prod;
            // meq.tail_next no change
         } else { // cur_new_prod > meq.tail
            if (!meq.tail_prev.empty()) {
               if ( cur_new_prod.name == meq.tail_prev.name) {
                  if (cur_new_prod.elected_votes > cur_old_prod.elected_votes) {
                     meq.tail_prev.clear();
                     refresh_main_tail_prev = true;
                  }
               } else {
                  if (cur_new_prod < meq.tail_prev) {
                     meq.tail_prev = cur_new_prod;
                  }
               }
            } else {
               meq.tail_prev = cur_new_prod;
            }
         }

      } else if (cur_new_prod > meq.tail) {
         if (_gstate.last_producer_schedule_size < main_producer_count) {
            ASSERT(beq.tail.empty());
            // TODO: add main producer change
            _gstate.last_producer_schedule_size++;
            if (!meq.tail_prev.empty()) {
               ASSERT(cur_new_prod.name != meq.tail_prev.name);
               if (cur_new_prod < meq.tail_prev) {
                  meq.tail_prev = cur_new_prod;
               }
            } else {
               meq.tail_prev = cur_new_prod;
            }
         } else {
            ASSERT(main_producer_count > 1 && !meq.tail_prev.empty());
            ASSERT(cur_new_prod.name != meq.tail_prev.name);
            auto old_main_producer_tail = meq.tail;
            if (cur_new_prod < meq.tail_prev) {
               meq.tail = cur_new_prod;
            } else {
               meq.tail = meq.tail_prev;
               meq.tail_prev.clear();
               refresh_main_tail_prev = true;
            }

            // TODO: add old_main_producer_tail to backup_producers
            meq.tail_next = old_main_producer_tail;
            // _gstate.ext->backup_producer_head = old_main_producer_tail;

            if (beq.last_producer_count > 0) {

               if (beq.last_producer_count == 1 && cur_name == beq.tail.name) {
                  ASSERT(beq.tail_prev.empty());
                  beq.tail = old_main_producer_tail;
                  // TODO: del curl prod from backup_producers;
               } else if (cur_name == beq.tail.name) {
                  beq.tail = beq.tail_prev;
                  beq.tail_prev.clear();
                  refresh_backup_tail_prev = true;
                  // TODO: del curl prod from backup_producers;
               } else if (cur_old_prod > beq.tail) {
                  ASSERT(!beq.tail_prev.empty());
                  if (beq.last_producer_count == 2) {
                     beq.tail_prev = old_main_producer_tail;
                  } else {
                     beq.tail_prev.clear();
                     refresh_backup_tail_prev = true;
                  }
                  // TODO: del curl prod from backup_producers;
               } else {
                  ASSERT(backup_producer_count > 1);
                  if (beq.last_producer_count == 1) {
                     ASSERT(beq.tail_prev.empty());
                     beq.tail = old_main_producer_tail;
                     beq.last_producer_count++;
                  } else if (beq.last_producer_count < backup_producer_count) {
                     ASSERT(!beq.tail_prev.empty());
                     beq.last_producer_count++;
                  } else {
                     ASSERT(!beq.tail_prev.empty());
                     // pop backup tail
                     // TODO: del backup tail
                     beq.tail_next = beq.tail;
                     beq.tail = beq.tail_prev;
                     beq.tail_prev.clear();
                     refresh_backup_tail_prev = true;
                  }
               }
            } else {
               meq.tail_next = old_main_producer_tail;
               // _gstate.ext->backup_producer_head = old_main_producer_tail;
               beq.tail = old_main_producer_tail;
               beq.last_producer_count++;
            }

         }

      } else {
         if (beq.last_producer_count > 0)
         if (cur_old_prod > beq.tail || cur_name == beq.tail.name) {
         }
         // check backup producers
         if (cur_new_prod.elected_votes > 0) {
            // cur prod is first backup producer


         }

      }
   }
            // _gstate.last_producer_schedule_size
      // if (prod_info.total_votes)
      // {
      //    /* code */
      // }

   }

} /// namespace eosiosystem
