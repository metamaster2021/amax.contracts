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

   namespace producer_change_helper {

      void add(const name& producer_name, const eosio::block_signing_authority  producer_authority, std::map<name, eosio::producer_change_record> &changes) {
         auto itr = changes.find(producer_name);
         if (itr != changes.end()) {
            auto op = (eosio::producer_change_operation)itr->second.index();
            switch (op) {
               case eosio::producer_change_operation::del :
                  itr->second = eosio::producer_authority_modify{producer_authority};
                  break;
               default:
                  CHECK(false, "the old change type can not be " + std::to_string((uint8_t)op) + " when add prod change")
                  break;
            }
         } else {
            changes.emplace(producer_name, eosio::producer_authority_add{producer_authority});
         }
      }

      void modify(const name& producer_name, const eosio::block_signing_authority  producer_authority, std::map<name, eosio::producer_change_record> &changes, const producer_info &prod_info) {
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
                  CHECK(false, "the old change type can not be " + std::to_string((uint8_t)op) + " when add prod change")
                  break;
            }
         } else {
            changes.emplace(producer_name, eosio::producer_authority_modify{producer_authority});
         }
      }

      void del(const name& producer_name, const eosio::block_signing_authority  producer_authority, std::map<name, eosio::producer_change_record> &changes) {
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
                  CHECK(false, "the old change type can not be " + std::to_string((uint8_t)op) + " when add prod change")
                  break;
            }
         } else {
            changes.emplace(producer_name, eosio::producer_authority_del{producer_authority});
         }
      }
   }

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
               // p.ext = producer_info_ext{
               //    .elected_votes = p.total_votes
               // };
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
            propagate_weight_change( *old_proxy, voter_name );
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
            propagate_weight_change( *new_proxy, voter_name );
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

      proposed_producer_changes changes;
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
               process_elected_producer(p, init_total_votes, p.total_votes, changes);
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

      if (!_gstate.ext.has_value() && (!changes.backup_changes.changes.empty() || !changes.main_changes.changes.empty()) ) {
         elected_change_table _elected_changes(get_self(), get_self().value);
         auto& ext = _gstate.ext.value();
         auto producer_change_id = ext.last_producer_change_id == 0 ? 1 : ext.last_producer_change_id + 1;
         _elected_changes.emplace( voter_name, [&]( auto& c ) {
               c.id        = producer_change_id;
               c.changes   = changes;
         });
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
            propagate_weight_change( proxy, payer );
         } else {

            proposed_producer_changes changes;
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
               process_elected_producer(prod, init_total_votes, init_total_votes + delta, changes);
            }

            if (!_gstate.ext.has_value() && (!changes.backup_changes.changes.empty() || !changes.main_changes.changes.empty()) ) {
               elected_change_table _elected_changes(get_self(), get_self().value);
               auto& ext = _gstate.ext.value();
               auto producer_change_id = ext.last_producer_change_id == 0 ? 1 : ext.last_producer_change_id + 1;
               _elected_changes.emplace( payer, [&]( auto& c ) {
                     c.id        = producer_change_id;
                     c.changes   = changes;
               });
            }
         }
      }
      _voters.modify( voter, same_payer, [&]( auto& v ) {
            v.last_vote_weight = new_weight;
         }
      );
   }

   void system_contract::process_elected_producer(const producer_info& prod_info, double old_votes, double new_votes, proposed_producer_changes &changes) {

      if (!_gstate.ext.has_value())
         return;

      auto &meq = _gstate.ext->main_elected_queue;
      auto &beq = _gstate.ext->backup_elected_queue;
      if (meq.last_producer_count == 0)
         return;


      const auto& cur_name = prod_info.owner;
      const auto& producer_authority = prod_info.producer_authority;
      producer_elected_votes cur_old_prod = {cur_name, old_votes, producer_authority};
      producer_elected_votes cur_new_prod = {cur_name, std::max(0.0, new_votes), producer_authority};

      // elected_change_table prod_change_tbl(get_self(), get_self().value);
      // elected_change change;
      auto &main_changes = changes.main_changes.changes;
      auto &backup_changes = changes.backup_changes.changes;

      bool refresh_main_tail_prev = false; // refresh by main_tail
      bool refresh_main_tail_next = false; // refresh by main_tail
      bool refresh_backup_tail_prev = false; // refresh by backup_tail
      bool refresh_backup_tail_next = false; // refresh by backup_tail

      // TODO: refresh all queue position info

      ASSERT(meq.last_producer_count > 0 && !meq.tail.empty());

      if (meq.last_producer_count == 1) { // && meq.last_producer_count > 0
         ASSERT(!meq.tail.empty() && meq.tail_prev.empty())
         if (cur_name == meq.tail.name) {
            meq.tail = cur_new_prod;
            // TODO: need to update cur prod authority to main queue?
         } else if (cur_new_prod.elected_votes > 0) {
            if (cur_new_prod > meq.tail) {
               meq.tail_prev = cur_new_prod;
            } else {
               meq.tail_prev = meq.tail;
               meq.tail = cur_new_prod;
            }

            producer_change_helper::add(cur_name, producer_authority, main_changes);
            meq.last_producer_count++;
         }
      } else if (cur_old_prod >= meq.tail) { // && meq.last_producer_count > 1
         ASSERT(!meq.tail_prev.empty())
         // producer was main producer and not main producer tail

         if (cur_new_prod > meq.tail_prev) {
            if (cur_name == meq.tail_prev.name) {
               if (meq.last_producer_count > 2) {
                  meq.tail_prev.clear();
                  refresh_main_tail_prev = true;
               }
            } else if (cur_name == meq.tail.name) {
               meq.tail = meq.tail_prev;
               meq.tail_prev.clear();
               refresh_main_tail_prev = true;
            }

         } else if (cur_new_prod > meq.tail) { // and cur_new_prod <= meq.tail_prev
            if (cur_name != meq.tail_prev.name && cur_name != meq.tail.name) {
               meq.tail_prev = cur_new_prod;
            }
         } else { //cur_new_prod <= meq.tail
            if (meq.tail_next.empty() || cur_new_prod > meq.tail_next) {
               if (meq.tail_next.empty()) ASSERT(beq.last_producer_count == 0 && beq.tail.empty())

               if (cur_name != meq.tail.name) {
                  meq.tail_prev = meq.tail;
                  meq.tail = cur_new_prod;
               }
            } else {// !meq.tail_next.empty() && cur_new_prod < meq.tail_next
               ASSERT(meq.last_producer_count == main_producer_count)
               // meq-, pop cur prod from main queue
               producer_change_helper::del(cur_name, producer_authority, main_changes);
               if (cur_name != meq.tail.name) {
                  meq.tail_prev = meq.tail;
               }
               meq.tail = meq.tail_next;
               ASSERT(beq.last_producer_count > 0 && !beq.tail.empty())
               // beq-: del main tail next from backup queue
               producer_change_helper::del(meq.tail_next.name, meq.tail_next.authority, backup_changes);
               // meq+: add main tail next to main queue
               producer_change_helper::add(meq.tail_next.name, meq.tail_next.authority, main_changes);

               if (cur_new_prod.elected_votes > 0 && (beq.tail_next.empty() || cur_new_prod > beq.tail_next) ) {
                  //beq+: add cur prod to backup queue
                  producer_change_helper::add(cur_name, producer_authority, backup_changes);
                  if (beq.last_producer_count == 1) {
                     ASSERT(beq.tail == meq.tail_next && beq.tail_prev.empty())
                     beq.tail = cur_new_prod;
                     meq.tail_next = cur_new_prod;
                  } else if (beq.last_producer_count == 2) {
                     ASSERT(!beq.tail_prev.empty() && beq.tail_prev == meq.tail_next)
                     // ASSERT(cur_new_prod.name != beq.tail.name)
                     if (cur_new_prod > beq.tail) {
                        beq.tail_prev = cur_new_prod;
                     } else { // cur_new_prod < beq.tail
                        beq.tail_prev = beq.tail;
                        beq.tail = cur_new_prod;
                     }
                     meq.tail_next = beq.tail_prev;

                  } else if (beq.last_producer_count == 3) {
                     ASSERT(!beq.tail_prev.empty() && !(beq.tail_prev == meq.tail_next))

                     if (cur_new_prod > beq.tail_prev) {
                        meq.tail_next = cur_new_prod;
                     } else if (cur_new_prod > beq.tail) {
                        meq.tail_next = beq.tail_prev;
                        beq.tail_prev = cur_new_prod;
                     } else { // cur_new_prod < beq.tail
                        meq.tail_next = beq.tail_prev;
                        beq.tail_prev = beq.tail;
                        beq.tail = cur_new_prod;
                     }
                  } else { // beq.last_producer_count > 3
                     ASSERT(!beq.tail_prev.empty() && !beq.tail.empty())
                     meq.tail_next.clear();
                     refresh_main_tail_next = true;
                     if (cur_new_prod < beq.tail) {
                        beq.tail_prev = beq.tail;
                        beq.tail = cur_new_prod;
                     } else if (cur_new_prod < beq.tail_prev) {
                        beq.tail_prev = cur_new_prod;
                     }
                     // no change: beq.last_producer_count
                  }
               } else { // cur_new_prod.elected_votes == 0 || cur_new_prod < beq.tail_next
                  if (beq.last_producer_count == 1) {
                     ASSERT(beq.tail == meq.tail_next && beq.tail_prev.empty())
                     beq.tail.clear();
                     meq.tail_next.clear();
                  } else if (beq.last_producer_count == 2) {
                     ASSERT(!beq.tail_prev.empty() && beq.tail_prev == meq.tail_next)
                     beq.tail_prev.clear();
                     meq.tail_next = beq.tail;
                  } else if (beq.last_producer_count == 3) {
                     ASSERT(!beq.tail_prev.empty() && !(beq.tail_prev == meq.tail_next))
                     meq.tail_next = beq.tail_prev;
                  } else if (!beq.tail_next.empty()) {
                     beq.tail_prev = beq.tail;
                     beq.tail = beq.tail_next;
                     beq.tail_next.clear();
                     refresh_backup_tail_next = true;
                  }
                  beq.last_producer_count--;
               }
            }
         }
      } else if (meq.last_producer_count < main_producer_count) { // && cur_old_prod < meq.tail
         ASSERT(cur_name != meq.tail.name && cur_name != meq.tail_prev.name)
         ASSERT(beq.last_producer_count == 0 && beq.tail.empty())
         if (cur_new_prod.elected_votes > 0) {
            // meq+: add cur prod to main queue
            producer_change_helper::add(cur_name, producer_authority, main_changes);
            meq.last_producer_count++;
            if (cur_new_prod < meq.tail) {
               meq.tail_prev = meq.tail;
               meq.tail = cur_new_prod;
            } else if (cur_new_prod < meq.tail_prev) {
               meq.tail_prev = cur_new_prod;
            }
         }
      } else { // meq.last_producer_count == main_producer_count && cur_old_prod < meq.tail
         if (cur_new_prod > meq.tail) {
            // meq-: pop main tail from main queue
            producer_change_helper::del(meq.tail.name, meq.tail.authority, main_changes);

            // ASSERT(cur_name != meq.tail.name && cur_name != meq.tail_prev.name)
            auto old_main_tail = meq.tail;
            if (cur_new_prod > meq.tail_prev) {
               meq.tail = meq.tail_prev;
               meq.tail_prev.clear();
               refresh_main_tail_prev = true;
            } else { // cur_new_prod > meq.tail
               meq.tail = cur_new_prod;
            }

            ASSERT(main_producer_count > 1);

            if (beq.last_producer_count == 0) {
               ASSERT(meq.tail.empty() && beq.tail_prev.empty() && meq.tail_next.empty())
               if (old_main_tail.elected_votes > 0) {
                  beq.tail = old_main_tail;
                  beq.last_producer_count++;
               }
            } else if (beq.last_producer_count == 1) {
               if (cur_old_prod == beq.tail) {
                  beq.tail = old_main_tail;
                  meq.tail_next = old_main_tail;
               } else {
                  meq.tail_next = old_main_tail;
                  beq.tail_prev = old_main_tail;
                  beq.last_producer_count++;
               }
            } else { // beq.last_producer_count > 1
               ASSERT(!meq.tail.empty() && !beq.tail_prev.empty())
               if (cur_old_prod >= beq.tail) {
                  if (cur_old_prod == beq.tail) {
                     beq.tail = beq.tail_prev;
                     if (beq.last_producer_count == 2) {
                        ASSERT(meq.tail_next == beq.tail_prev)
                        beq.tail_prev = old_main_tail;
                     } else if (beq.last_producer_count == 3) {
                        ASSERT(!(meq.tail_next == beq.tail_prev))
                        beq.tail_prev = meq.tail_next;
                     } else { // beq.last_producer_count > 3
                        beq.tail_prev.clear();
                        refresh_backup_tail_prev = true;
                     }
                  } else if (cur_old_prod == beq.tail_prev) {
                     if (beq.last_producer_count == 2) {
                        ASSERT(meq.tail_next == beq.tail_prev)
                        beq.tail_prev = old_main_tail;
                     } else if (beq.last_producer_count == 3) {
                        ASSERT(!(meq.tail_next != beq.tail_prev))
                        beq.tail_prev = meq.tail_next;
                     } else {
                        beq.tail_prev.clear();
                        refresh_backup_tail_prev = true;
                     }
                  } else { // cur_old_prod > beq.tail_prev
                     ASSERT(beq.last_producer_count > 3)
                  }
               } else { // cur_old_prod < beq.tail
                  meq.tail_next = old_main_tail;
                  if (beq.last_producer_count == backup_producer_count) {
                     // beq-: pop backup tail from backup queue
                     producer_change_helper::del(beq.tail.name, beq.tail.authority, backup_changes);
                     beq.tail = beq.tail_prev;
                     beq.tail_prev.clear();
                     refresh_backup_tail_prev = true;
                  } else  {
                     beq.last_producer_count++;
                  }
               }
            }

            // process old_main_tail
            if (old_main_tail.elected_votes != 0) {
               // beq+: push old_main_tail to backup queue
               producer_change_helper::add(old_main_tail.name, old_main_tail.authority, backup_changes);
               meq.tail_next = old_main_tail;
            } else {
               ASSERT(meq.tail_next.empty() && beq.last_producer_count == 0);
            }

         } else { // else cur_old_prod < meq.tail && cur_new_prod < meq.tail

            // if (beq.last_producer_count == 0) {
            //    if (cur_new_prod.elected_votes > 0) {
            //       beq.tail = cur_new_prod;
            //       meq.tail_next = cur_new_prod;
            //       beq.last_producer_count++;
            //       // TODO: push cur prod to backup queue
            //    }
            // } else if (beq.last_producer_count == 1) {
            //    if (cur_name == beq.tail.name) {
            //       beq.tail = cur_new_prod;
            //    }

            // }
            ASSERT(backup_producer_count > 3)

            if (beq.last_producer_count == 0) {
               if (cur_new_prod.elected_votes > 0) {
                  beq.tail = cur_new_prod;
                  meq.tail_next = cur_new_prod;
                  beq.last_producer_count++;
                  // beq+: push cur prod to backup queue
                  producer_change_helper::add(cur_name, producer_authority, backup_changes);
               }
            } else if (beq.last_producer_count == 1) {
               if (cur_new_prod.elected_votes > 0) {
                  if (cur_name == beq.tail.name) {
                     beq.tail = cur_new_prod;
                     meq.tail_next = cur_new_prod;
                  } else {
                     if (cur_new_prod > beq.tail) {
                        beq.tail_prev = cur_new_prod;
                        meq.tail_next = cur_new_prod;
                     } else { // cur_old_prod < beq.tail
                        beq.tail_prev = beq.tail;
                        meq.tail_next = beq.tail;
                        beq.tail = cur_new_prod;
                     }
                     beq.last_producer_count++;
                     // beq+: add cur prod to backup queue
                     producer_change_helper::add(cur_name, producer_authority, backup_changes);
                  }
               } else { // cur_new_prod.elected_votes <= 0
                  if (cur_name == beq.tail.name) {
                     beq.tail.clear();
                     meq.tail_next.clear();
                     // beq-: del cur prod from backup queue
                     producer_change_helper::add(cur_name, producer_authority, backup_changes);
                  }
               }

            } else { // beq.last_producer_count > 1
               ASSERT(!beq.tail.empty() && !beq.tail_prev.empty() && !meq.tail_next.empty())
               if (cur_old_prod >= beq.tail) {
                  if (beq.last_producer_count == 2) {
                     ASSERT(beq.tail_prev == meq.tail_next)
                     if (cur_old_prod == beq.tail_prev) {
                        if (cur_new_prod > beq.tail) {
                           beq.tail_prev = cur_new_prod;
                           meq.tail_next = cur_new_prod;
                        } else if (cur_new_prod.elected_votes > 0) { // cur_old_prod < beq.tail
                           beq.tail_prev = meq.tail;
                           meq.tail_next = meq.tail;
                           beq.tail = cur_new_prod;
                        } else { // cur_new_prod.elected_votes <= 0
                           beq.tail_prev.clear();
                           meq.tail_next = meq.tail;
                        }
                     } else if (cur_old_prod == beq.tail) {
                        if (cur_new_prod > beq.tail_prev) {
                           beq.tail = beq.tail_prev;
                           beq.tail_prev = cur_new_prod;
                           meq.tail_next = cur_new_prod;
                        } else if (cur_new_prod.elected_votes > 0) { // && cur_new_prod < beq.tail_prev
                           beq.tail = cur_new_prod;
                        } else { // cur_new_prod.elected_votes <= 0
                           beq.tail = beq.tail_prev;
                           meq.tail_next = beq.tail_prev;
                           beq.tail_prev.clear();
                           beq.last_producer_count--;
                           // beq-: del cur prod from backup queue
                           producer_change_helper::del(cur_name, producer_authority, backup_changes);
                        }
                     }
                  } else if (beq.last_producer_count == 3) {
                     ASSERT(beq.tail_prev != meq.tail_next)

                     if (cur_old_prod == meq.tail_next) {
                        if (cur_new_prod > beq.tail_prev) {
                           meq.tail_next = cur_new_prod;
                        } else if (cur_new_prod > beq.tail) {
                           meq.tail_next = beq.tail_prev;
                           beq.tail_prev = cur_new_prod;
                        } else if (cur_new_prod.elected_votes > 0) { // && cur_new_prod < beq.tail
                           meq.tail_next = beq.tail_prev;
                           beq.tail_prev = meq.tail;
                           meq.tail = cur_new_prod;
                        } else { // cur_new_prod.elected_votes <= 0
                           meq.tail_next = beq.tail_prev;
                           beq.last_producer_count--;
                           // beq-: del cur prod from backup queue
                           producer_change_helper::del(cur_name, producer_authority, backup_changes);
                        }
                     } else if (cur_old_prod == beq.tail_prev) {
                        if (cur_new_prod > meq.tail_next) {
                           beq.tail_prev = meq.tail_next;
                           meq.tail_next = cur_new_prod;
                        } else if (cur_new_prod > beq.tail) { // && cur_new_prod < beq.tail_prev
                           beq.tail_prev = cur_new_prod;
                        } else if (cur_new_prod.elected_votes > 0) { // && cur_new_prod < beq.tail
                           beq.tail_prev = meq.tail;
                           meq.tail = cur_new_prod;
                        } else { // cur_new_prod.elected_votes <= 0
                           beq.tail_prev = meq.tail_next;
                           beq.last_producer_count--;
                           // beq-: del cur prod from backup queue
                           producer_change_helper::del(cur_name, producer_authority, backup_changes);
                        }
                     } else if (cur_old_prod == beq.tail) {
                        if (cur_new_prod > meq.tail_next) {
                           beq.tail = beq.tail_prev;
                           beq.tail_prev = meq.tail_next;
                           meq.tail_next = cur_new_prod;
                        } else if (cur_new_prod > beq.tail_prev) { // && cur_new_prod < meq.tail_next
                           beq.tail = beq.tail_prev;
                           beq.tail_prev = cur_new_prod;
                        } else if (cur_new_prod.elected_votes > 0) { // && cur_new_prod < beq.tail
                           meq.tail = cur_new_prod;
                        } else { // cur_new_prod.elected_votes <= 0
                           beq.tail = beq.tail_prev;
                           beq.tail_prev = meq.tail_next;
                           beq.last_producer_count--;
                           // beq-: del cur prod from backup queue
                           producer_change_helper::del(cur_name, producer_authority, backup_changes);
                        }
                     }
                  } else { // beq.last_producer_count > 3
                     if (cur_new_prod >= meq.tail_next) {
                        if (cur_old_prod == beq.tail_prev) {
                           beq.tail_prev.clear();
                           refresh_backup_tail_prev = true;
                        } else if (cur_old_prod == beq.tail) {
                           beq.tail = beq.tail_prev;
                           beq.tail_prev.clear();
                           refresh_backup_tail_prev = true;
                        }
                        meq.tail_next = cur_new_prod;
                     } else if (cur_new_prod >= beq.tail_prev) { // && cur_new_prod < meq.tail_next
                        if (cur_old_prod == meq.tail_next) {
                           meq.tail_next.clear();
                           refresh_main_tail_next = true;
                        } else if (cur_old_prod == beq.tail_prev) {
                           if (cur_new_prod != beq.tail_prev) {
                              beq.tail_prev.clear();
                              refresh_backup_tail_prev = true;
                           }
                        } else if (cur_old_prod == beq.tail) {
                           beq.tail = beq.tail_prev;
                           beq.tail_prev.clear();
                           refresh_backup_tail_prev = true;
                        }
                     } else if (cur_new_prod >= beq.tail) { // cur_new_prod < beq.tail_prev
                        if (cur_old_prod != beq.tail_prev && cur_old_prod == beq.tail) {
                           beq.tail_prev = cur_new_prod;
                           if (cur_old_prod == meq.tail_next) {
                              meq.tail_next.clear();
                              refresh_main_tail_next = true;
                           }
                        }

                     // cur_new_prod < beq.tail && cur_new_prod.elected_votes > 0
                     } else if ( (beq.tail_prev.empty() && cur_new_prod.elected_votes > 0) ||
                                 (!beq.tail_next.empty() && cur_new_prod > beq.tail_next )) {
                        if (cur_old_prod == meq.tail_next) {
                           meq.tail_next.clear();
                           refresh_main_tail_next = true;
                           beq.tail = cur_new_prod;
                        } else if (cur_old_prod == beq.tail_prev) {
                           beq.tail_prev = beq.tail;
                           beq.tail = cur_new_prod;
                        } else if (cur_old_prod == beq.tail) {
                           beq.tail = cur_new_prod;
                        } else {
                           beq.tail_prev = beq.tail;
                           beq.tail = cur_new_prod;
                        }
                     } else if (!beq.tail_next.empty() && cur_new_prod < beq.tail_next ) {
                        ASSERT(beq.last_producer_count == backup_producer_count)
                        if (cur_old_prod >= beq.tail_prev) {
                           beq.tail_prev = beq.tail;
                           if (cur_old_prod == meq.tail_next) {
                              meq.tail_next.clear();
                              refresh_main_tail_next = true;
                           }
                        }

                        // beq-: del cur prod from backup queue
                        producer_change_helper::del(cur_name, producer_authority, backup_changes);
                        // beq+: add beq.tail_next to backup queue
                        producer_change_helper::add(beq.tail_next.name, beq.tail_next.authority, backup_changes);
                        beq.tail = beq.tail_next;
                        beq.tail_next.clear();
                        refresh_backup_tail_next = true;
                     } else { // beq.tail_prev.empty() && cur_new_prod.elected_votes <= 0)
                        beq.last_producer_count--;
                        // beq-: del cur prod from backup queue
                        producer_change_helper::del(cur_name, producer_authority, backup_changes);
                        if (cur_old_prod == meq.tail_next) {
                           meq.tail_next.clear();
                           refresh_main_tail_next = true;
                        } else if (cur_old_prod == beq.tail_prev) {
                           beq.tail_prev.clear();
                           refresh_backup_tail_prev = true;
                        } else if (cur_old_prod == beq.tail) {
                           beq.tail = beq.tail_prev;
                           beq.tail_prev.clear();
                           refresh_backup_tail_prev = true;
                        }
                     }
                  }
               } else { // cur_old_prod < beq.tail

                  if (beq.last_producer_count < backup_producer_count) {
                     ASSERT(beq.tail_next.empty());
                     if (cur_new_prod.elected_votes > 0) {
                        beq.last_producer_count++;
                        if (cur_new_prod < beq.tail) {
                           beq.tail_prev = beq.tail;
                           beq.tail = cur_new_prod;
                        } else if (cur_new_prod < beq.tail_prev) {
                           beq.tail_prev = cur_new_prod;
                        } else if (cur_new_prod > meq.tail_next) {
                           meq.tail_next = cur_new_prod;
                        }
                     }
                  } else { // beq.last_producer_count == backup_producer_count

                     if (cur_new_prod > beq.tail) {
                        // beq-: del beq.tail from backup queue
                        producer_change_helper::del(beq.tail.name, beq.tail.authority, backup_changes);
                        beq.tail_next = beq.tail;
                        if (cur_new_prod < beq.tail_prev) { // && cur_new_prod >= beq.tail
                           beq.tail = cur_new_prod;
                        } else { // cur_new_prod > meq.tail_prev
                           beq.tail = beq.tail_prev;
                           beq.tail_prev.clear();
                           refresh_backup_tail_prev = true;
                           if (cur_new_prod > meq.tail_prev) {
                              meq.tail_next = cur_new_prod;
                           }
                        }

                     } else { // cur_new_prod < beq.tail)
                        if (  (beq.tail_next.empty() && cur_new_prod.elected_votes > 0) ||
                              (!beq.tail_next.empty() && cur_new_prod > beq.tail_next) ) {
                           beq.tail_next = cur_new_prod;
                        }
                     }
                  }
               }
            }
         }
      }
   }

} /// namespace eosiosystem
