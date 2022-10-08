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
   using eosio::producer_authority_add;
   using eosio::producer_authority_modify;
   using eosio::producer_authority_del;
   using eosio::producer_change_map;
   using eosio::print;
   using std::to_string;
   using std::string;

   // inline bool operator<(const producer_elected_votes& a, const producer_elected_votes& b);
   // inline bool operator>(const producer_elected_votes& a, const producer_elected_votes& b);
   // inline bool operator<=(const producer_elected_votes& a, const producer_elected_votes& b);
   // inline bool operator>=(const producer_elected_votes& a, const producer_elected_votes& b);
   // inline bool operator==(const producer_elected_votes& a, const producer_elected_votes& b);
   // inline bool operator!=(const producer_elected_votes& a, const producer_elected_votes& b);

   static constexpr uint32_t min_backup_producer_count = 3;
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
                  CHECK(false, "the old change type can not be " + std::to_string((uint8_t)op) + " when add prod change: " + producer_name.to_string())
                  break;
            }
         } else {
            changes.emplace(producer_name, eosio::producer_authority_add{producer_authority});
         }
      }

      void modify(const name& producer_name, const eosio::block_signing_authority  producer_authority, std::map<name, eosio::producer_change_record> &changes) {
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

      void del(const name& producer_name, std::map<name, eosio::producer_change_record> &changes) {
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

      void merge(const producer_change_map& src, producer_change_map& dest) {
         ASSERT(!src.clear_existed && !dest.clear_existed);
         for (const auto& c : src.changes) {
            std::visit(
               overloaded {
                  [&prod_name=c.first, &dest_changes=dest.changes](const producer_authority_add& v) {
                     add(prod_name, *v.authority, dest_changes);
                  },
                  [&prod_name=c.first, &dest_changes=dest.changes](const producer_authority_modify& v) {
                     modify(prod_name, *v.authority, dest_changes);
                  },
                  [&prod_name=c.first, &dest_changes=dest.changes](const producer_authority_del& v) {
                     del(prod_name, dest_changes);
                  }},
               c.second);
         }
         dest.producer_count = src.producer_count;
      }

      void merge(const proposed_producer_changes& src, proposed_producer_changes& dest) {
         ASSERT(!src.main_changes.clear_existed && !src.backup_changes.clear_existed)
         merge(src.main_changes, dest.main_changes);
         merge(src.backup_changes, dest.backup_changes);
      }
   }

   namespace queue_helper {

      template<typename index_t>
      auto find_pos(index_t &idx, const producer_elected_votes &prod, const char* title) {
         auto itr = idx.lower_bound(producer_info::by_votes_prod(prod.name, prod.elected_votes, true));

         size_t steps = 1;
         while(true) {
            // if (steps >= 3) itr = idx.end();
            CHECK(itr != idx.end(), string(title) + ", pos not found! steps:" + to_string(steps) +
               " expectd:" + prod.name.to_string() + ":" + to_string(prod.elected_votes));
            if (itr->owner == prod.name) {
               print("found position of prod:", prod.name, ":", prod.elected_votes, " steps:", steps, "\n");
               break;
            }
            CHECK(itr->get_elected_votes() == prod.elected_votes && itr->owner < prod.name,
               string(title) + ", prod mismatch in db! steps:" + to_string(steps) +
               " expectd:" + prod.name.to_string() + ":" + to_string(prod.elected_votes) +
               " got:" + itr->owner.to_string() + ":" + to_string(itr->get_elected_votes()));
            steps++;
            itr++;
         };
         return itr;
      }

      template<typename index_t>
      void fetch_prev(index_t &idx, const producer_elected_votes &tail, producer_elected_votes &prev, bool check_found, const char* title) {
         auto itr = find_pos(idx, tail, title);
         auto begin = idx.begin();
         check(begin != idx.end(), "totalvotepro index of producer table is empty");
         if (itr != begin) {
            itr--;
            prev = {itr->owner, itr->total_votes, itr->producer_authority};
            ASSERT(tail < prev);
            eosio::print(title, " updated: ", itr->owner, ":", itr->total_votes, "\n");
         } else {
            if (check_found) {
               CHECK(false, string(title) + " not found! tail: " + tail.name.to_string() + ":" + to_string(tail.elected_votes))
            }
            eosio::print(title, " cleared\n");
            prev.clear();
         }
      }

      template<typename index_t>
      void fetch_next(index_t &idx, const producer_elected_votes &tail, producer_elected_votes &next, bool check_found, const char* title) {
         auto itr = find_pos(idx, tail, title);
         itr++;
         if (itr != idx.end() && itr->ext) {
            next = {itr->owner, itr->total_votes, itr->producer_authority};
            ASSERT(next < tail);
            eosio::print(title, " updated: ", itr->owner, ":", itr->total_votes, "\n");
         } else {
            if (check_found) {
               CHECK(false, string(title) + " not found! tail: " + tail.name.to_string() + ":" + to_string(tail.elected_votes))
            }
            eosio::print(title, " cleared\n");
            next.clear();
         }
      }

   }

   // inline long double by_votes_prod(const producer_elected_votes& v) {
   //    return producer_info::by_votes_prod(v.name, v.elected_votes);
   // }

   // inline bool operator<(const producer_elected_votes& a, const producer_elected_votes& b)  {
   //    return by_votes_prod(a) > by_votes_prod(b);
   // }

   // inline bool operator>(const producer_elected_votes& a, const producer_elected_votes& b)  {
   //    return by_votes_prod(a) < by_votes_prod(b);
   // }

   // inline bool operator<=(const producer_elected_votes& a, const producer_elected_votes& b)  {
   //       return !(a > b);
   // }
   // inline bool operator>=(const producer_elected_votes& a, const producer_elected_votes& b)  {
   //    return !(a < b);
   // }
   // inline bool operator==(const producer_elected_votes& a, const producer_elected_votes& b)  {
   //    return a.name == b.name && a.elected_votes == b.elected_votes;
   // }
   // inline bool operator!=(const producer_elected_votes& a, const producer_elected_votes& b)  {
   //    return !(a == b);
   // }

   void system_contract::initelects( const name& payer, uint32_t max_backup_producer_count ) {
      require_auth( payer );
      check(max_backup_producer_count >= min_backup_producer_count, "max_backup_producer_count must >= " + to_string(min_backup_producer_count));
      check(!_gstate.ext.has_value(), "elected producer has been initialized");

      auto block_time = current_block_time();

      _gstate.last_producer_schedule_update = block_time;
      eosio::proposed_producer_changes changes;
      changes.main_changes.clear_existed = true;
      changes.backup_changes.clear_existed = true;

      auto idx = _producers.get_index<"prototalvote"_n>();
      amax_global_state_ext ext;
      ext.elected_version = 1;
      ext.max_backup_producer_count = max_backup_producer_count;
      auto& meq = ext.main_elected_queue;
      auto& beq = ext.backup_elected_queue;
      auto &main_changes = changes.main_changes;
      auto &backup_changes = changes.backup_changes;

      check(_elected_changes.begin() == _elected_changes.end(), "elected change table is not empty" );

      // TODO: need using location to order producers?
      for( auto it = idx.cbegin(); it != idx.cend() && 0 < it->total_votes && it->active(); ++it ) {
         idx.modify( it, payer, [&]( auto& p ) {
            p.update_elected_votes();
         });
         if (main_changes.changes.size() < ext.max_main_producer_count) {
            main_changes.changes.emplace(
               it->owner, eosio::producer_authority_add {
                  .authority = it->producer_authority
               }
            );
            if (!meq.tail.empty()) {
               meq.tail_prev = meq.tail;
            }
            meq.tail = {it->owner, it->total_votes, it->producer_authority};

            ASSERT(meq.tail_prev.empty() || meq.tail_prev > meq.tail);
         } else if (backup_changes.changes.size() < min_backup_producer_count) {
            backup_changes.changes.emplace(
               it->owner, eosio::producer_authority_add {
                  .authority = it->producer_authority
               }
            );
            if (!beq.tail.empty()) {
               beq.tail_prev = beq.tail;
            }
            beq.tail = {it->owner, it->total_votes, it->producer_authority};

            if (meq.tail_next.empty()) {
               meq.tail_next = beq.tail;
               ASSERT(meq.tail > meq.tail_next && meq.tail_next > beq.tail_prev);
            }
            ASSERT(beq.tail_prev.empty() || beq.tail_prev > beq.tail);
         } else if (backup_changes.changes.size() == 3) {
            beq.tail_next = {it->owner, it->total_votes, it->producer_authority};
            break;
         }
      }
      main_changes.producer_count = main_changes.changes.size();
      meq.last_producer_count =  main_changes.changes.size();
      backup_changes.producer_count = backup_changes.changes.size();
      beq.last_producer_count =  backup_changes.changes.size();
      uint32_t min_producer_count = ext.max_main_producer_count + min_backup_producer_count + 1;

      CHECK(main_changes.producer_count + backup_changes.producer_count + 1 >= min_producer_count,
            "there must be at least " + to_string(min_producer_count) + " valid producers");

      auto ret = set_proposed_producers_ex( changes );
      CHECK(ret >= 0, "set proposed producers to native system failed(" + std::to_string(ret) + ")");

      _gstate.ext = ext;
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
         auto old_elected_votes = prod->get_elected_votes();
         _producers.modify( prod, producer, [&]( producer_info& info ){
            info.producer_key       = producer_key;
            info.is_active          = true;
            info.url                = url;
            info.location           = location;
            info.producer_authority = producer_authority;
            info.update_elected_votes();

            if ( info.last_claimed_time == time_point() )
               info.last_claimed_time = ct;
         });
         if (_gstate.is_init_elects()) {
            // TODO: update authority??
            auto new_elected_votes = prod->get_elected_votes();

            // if (old_elected_votes != new_elected_votes) {
               proposed_producer_changes changes;
               process_elected_producer(*prod, old_elected_votes, new_elected_votes, changes);

               if ( !changes.backup_changes.changes.empty() || !changes.main_changes.changes.empty() ) {
                  auto& ext = _gstate.ext.value();
                  auto producer_change_id = ++ext.last_producer_change_id;
                  _elected_changes.emplace( producer, [&]( auto& c ) {
                        c.id        = producer_change_id;
                        c.changes   = changes;
                  });
               }
            // }
         }
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

      static const uint32_t max_flush_elected_rows = 10;
      static const uint32_t min_flush_elected_changes = 300;

      if (_gstate.ext.has_value()) {
         auto &ext = _gstate.ext.value();
         proposed_producer_changes changes;
         auto itr = _elected_changes.rbegin();
         for (auto itr = _elected_changes.begin(); itr != _elected_changes.end(); ++itr) {
            producer_change_helper::merge(itr->changes, changes);
            if (changes.main_changes.changes.size() + changes.backup_changes.changes.size() >= min_flush_elected_changes ) {
               break;
            }
         }
         eosio::set_proposed_producers(changes);
         return;
      }

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
            double old_elected_votes = pitr->get_elected_votes();
            _producers.modify( pitr, same_payer, [&]( auto& p ) {
               p.total_votes += pd.second.first;
               if ( p.total_votes < 0 ) { // floating point arithmetics can give small negative numbers
                  p.total_votes = 0;
               }
               p.update_elected_votes();
               _gstate.total_producer_vote_weight += pd.second.first;
               //check( p.total_votes >= 0, "something bad happened" );
            });
            process_elected_producer(*pitr, old_elected_votes, pitr->get_elected_votes(), changes);
         } else {
            if( pd.second.second ) {
               check( false, ( "producer " + pd.first.to_string() + " is not registered" ).data() );
            }
         }
      }

      if ( !changes.backup_changes.changes.empty() || !changes.main_changes.changes.empty() ) {
         auto& ext = _gstate.ext.value();
         auto producer_change_id = ++ext.last_producer_change_id;
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
               const double old_elected_votes = prod.get_elected_votes();
               _producers.modify( prod, same_payer, [&]( auto& p ) {
                  p.total_votes += delta;
                  if ( p.total_votes < 0 ) { // floating point arithmetics can give small negative numbers
                     p.total_votes = 0;
                  }
                  p.update_elected_votes();
                  _gstate.total_producer_vote_weight += delta;
               });

               process_elected_producer(prod, old_elected_votes, prod.get_elected_votes(), changes);
            }

            if ( !changes.backup_changes.changes.empty() || !changes.main_changes.changes.empty() ) {
               auto& ext = _gstate.ext.value();
               auto producer_change_id = ++ext.last_producer_change_id;
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

      if (!_gstate.is_init_elects())
         return;

      // TODO: check producer count
      // TODO: min backup count

      auto &ext  = _gstate.ext.value();
      auto &meq = ext.main_elected_queue;
      auto &beq = ext.backup_elected_queue;
      const auto& cur_name = prod_info.owner;
      const auto& producer_authority = prod_info.producer_authority;

      eosio::print("***** beq.last_producer_count=", beq.last_producer_count, "\n");
      eosio::print("cur prod: ", cur_name, ",", new_votes, ",", old_votes,  "\n");
      eosio::print("meq tail_prev: ", meq.tail_prev.name, ",", meq.tail_prev.elected_votes, ",", "\n");
      eosio::print("meq tail: ", meq.tail.name, ",", meq.tail.elected_votes, ",", "\n");
      eosio::print("meq tail_next: ", meq.tail_next.name, ",", meq.tail_next.elected_votes, ",", "\n");
      eosio::print("beq tail_prev: ", beq.tail_prev.name, ",", beq.tail_prev.elected_votes, ",", "\n");
      eosio::print("beq tail: ", beq.tail.name, ",", beq.tail.elected_votes, ",", "\n");
      eosio::print("beq tail_next: ", beq.tail_next.name, ",", beq.tail_next.elected_votes, ",", "\n");

      auto min_producer_count = ext.max_main_producer_count + min_backup_producer_count + 1;
      ASSERT(meq.last_producer_count + beq.last_producer_count + 1 >= min_producer_count);
      ASSERT(!meq.tail.empty() && !meq.tail_prev.empty() && !beq.tail_next.empty() &&
             !beq.tail.empty() && !beq.tail_prev.empty() && !beq.tail_next.empty() && beq.tail_prev < meq.tail_next);

      producer_elected_votes cur_old_prod = {cur_name, old_votes, producer_authority};
      producer_elected_votes cur_new_prod = {cur_name, new_votes, producer_authority};

      // elected_change_table prod_change_tbl(get_self(), get_self().value);
      // elected_change change;
      auto &main_changes = changes.main_changes.changes;
      auto &backup_changes = changes.backup_changes.changes;

      bool refresh_main_tail_prev = false; // refresh by main_tail
      bool refresh_main_tail_next = false; // refresh by main_tail
      bool refresh_backup_tail_prev = false; // refresh by backup_tail
      bool refresh_backup_tail_next = false; // refresh by backup_tail

      // refresh queue positions
      auto idx = _producers.get_index<"totalvotepro"_n>();

      ASSERT(meq.last_producer_count > 0 && !meq.tail.empty());

      if (cur_old_prod >= meq.tail) { //
         ASSERT(!meq.tail_prev.empty())
         // producer was main producer and not main producer tail

         if (cur_new_prod >= meq.tail_prev) {
            if (cur_name == meq.tail_prev.name) {
               meq.tail_prev.clear();
               refresh_main_tail_prev = true;
            } else if (cur_name == meq.tail.name) {
               meq.tail = meq.tail_prev;
               meq.tail_prev.clear();
               refresh_main_tail_prev = true;
            }

         } else if (cur_new_prod >= meq.tail) { // and cur_new_prod <= meq.tail_prev
            if (cur_name == meq.tail.name) {
               meq.tail = cur_new_prod;
            } else if (cur_name == meq.tail_prev.name) {
               meq.tail_prev = cur_new_prod;
            } else {
               meq.tail_prev.clear();
               refresh_main_tail_prev = true;
            }
         } else if (cur_new_prod > meq.tail_next) { //cur_new_prod < meq.tail
            if (cur_name != meq.tail.name) {
               meq.tail_prev = meq.tail;
            }
            meq.tail = cur_new_prod;
         } else {// cur_new_prod < meq.tail_next

            // meq-, pop cur prod from main queue
            producer_change_helper::del(cur_name, main_changes);
            // beq-: del main tail next from backup queue
            producer_change_helper::del(meq.tail_next.name, backup_changes);
            // meq+: add main tail next to main queue
            producer_change_helper::add(meq.tail_next.name, meq.tail_next.authority, main_changes);
            if (cur_name != meq.tail.name) {
               meq.tail_prev = meq.tail;
            }
            meq.tail = meq.tail_next;
            ASSERT(beq.last_producer_count > 0 && !beq.tail.empty())

            if (cur_new_prod > beq.tail) {
               if (cur_new_prod < beq.tail_prev) {
                  beq.tail_prev = cur_new_prod;
               }
               // beq+: add cur prod to backup queue
               producer_change_helper::add(cur_name, producer_authority, backup_changes);
            } else if (cur_new_prod > beq.tail_next) { // cur_new_prod < beq.tail
               if (cur_new_prod.elected_votes > 0 || beq.last_producer_count == 3) {
                  beq.tail_prev = beq.tail;
                  beq.tail = cur_new_prod;
                  // beq+: add cur prod to backup queue
                  producer_change_helper::add(cur_name, producer_authority, backup_changes);
               } else { // cur_new_prod.elected_votes <= 0 && beq.last_producer_count
                  beq.tail_next = cur_new_prod;
                  beq.last_producer_count--;
               }
            } else { // cur_new_prod < beq.tail_next
               if(beq.tail_next.elected_votes > 0 || beq.last_producer_count == 3) {
                  beq.tail_prev = beq.tail;
                  beq.tail = beq.tail_next;
                  beq.tail_next.clear();
                  refresh_backup_tail_next = true;
               } else { // cur_new_prod < beq.tail_next
                  beq.last_producer_count--;
               }
            }
            meq.tail_next.clear();
            refresh_main_tail_next = true;
         }
      } else if (cur_new_prod > meq.tail) { // cur_old_prod < meq.tail

         // meq-: del meq.tail from main queue
         producer_change_helper::del(meq.tail.name, main_changes);
         // meq+: add cur prod to main queue
         producer_change_helper::add(cur_name, producer_authority, main_changes);
         // beq+: add meq.tail to backup queue
         producer_change_helper::add(meq.tail.name, meq.tail.authority, backup_changes);

         meq.tail_next = meq.tail;
         if (cur_new_prod > meq.tail_prev) {
            meq.tail = meq.tail_prev;
            meq.tail_prev.clear();
            refresh_main_tail_prev = true;
         } else { // cur_new_prod < meq.tail_prev && cur_new_prod > meq.tail
            meq.tail = cur_new_prod;
         }

         if (cur_old_prod >= beq.tail) {
            // beq-: del cur prod from backup queue
            producer_change_helper::del(cur_name, backup_changes);
            if (cur_old_prod == beq.tail_prev) {
                  beq.tail_prev.clear();
                  refresh_backup_tail_prev = true;
            } else if (cur_old_prod == beq.tail) {
               beq.tail = beq.tail_prev;
               beq.tail_prev.clear();
               refresh_backup_tail_prev = true;
            }
         } else { // cur_old_prod < beq.tail
            bool is_pop_tail = false;
            if (beq.last_producer_count == ext.max_backup_producer_count) {
               is_pop_tail = true;
            } else { // beq.last_producer_count < ext.max_backup_producer_count
               if (cur_old_prod == beq.tail_next) {
                  queue_helper::fetch_next(idx, beq.tail, beq.tail_next, false, "backup queue tail next");
                  is_pop_tail = beq.tail_next.empty();
               }
            }

            if (is_pop_tail) {
               // beq-: pop backup tail from backup queue
               producer_change_helper::del(beq.tail.name, backup_changes);
               beq.tail_next = beq.tail;
               beq.tail = beq.tail_prev;
               beq.tail_prev.clear();
               refresh_backup_tail_prev = true;
            } else {
               beq.last_producer_count++;
            }
         }
      } else if (cur_old_prod >= beq.tail) { // && cur_old_prod < meq.tail && cur_new_prod < meq.tail

         if (cur_new_prod >= meq.tail_next) {
            if (cur_old_prod == beq.tail_prev) {
               refresh_backup_tail_prev = true;
            } else if (cur_old_prod == beq.tail) {
               beq.tail = beq.tail_prev;
               refresh_backup_tail_prev = true;
            }
            meq.tail_next = cur_new_prod;
         } else if (cur_new_prod >= beq.tail_prev) { // && cur_new_prod < meq.tail_next
            if (cur_old_prod == meq.tail_next) {
               refresh_main_tail_next = true;
            } else if (cur_old_prod == beq.tail_prev) {
               if (cur_new_prod != beq.tail_prev) {
                  refresh_backup_tail_prev = true;
               }
            } else if (cur_old_prod == beq.tail) {
               beq.tail = beq.tail_prev;
               refresh_backup_tail_prev = true;
            }
         } else if (cur_new_prod >= beq.tail) { // && cur_new_prod < beq.tail_prev
            if (cur_old_prod == beq.tail) {
               beq.tail = cur_new_prod;
            } else { // cur_old_prod != beq.tail {
               beq.tail_prev = cur_new_prod;
               if (cur_old_prod == meq.tail_next) {
                  meq.tail_next.clear();
                  refresh_main_tail_next = true;
               }
            }

         } else if ( cur_new_prod > beq.tail_next ) { // && cur_new_prod < beq.tail
            if (cur_new_prod.elected_votes > 0 || beq.last_producer_count == 3) {
               if (cur_old_prod != beq.tail) {
                  beq.tail_prev = beq.tail;
                  if (cur_old_prod == meq.tail_next) {
                     refresh_main_tail_next = true;
                  }
               }
               beq.tail = cur_new_prod;
            } else { // cur_new_prod.elected_votes <= 0 && beq.last_producer_count > 3
               if (cur_old_prod == beq.tail) {
                  beq.tail = beq.tail_prev;
                  refresh_backup_tail_prev = true;
               } else if (cur_old_prod == beq.tail_prev) {
                  refresh_backup_tail_prev = true;
               } else if (cur_old_prod == meq.tail_next) {
                  refresh_main_tail_next = true;
               }

               beq.tail_next = cur_new_prod;
               beq.last_producer_count--;
               // beq-: del cur prod from backup queue
               producer_change_helper::del(cur_name, backup_changes);
            }
         } else { // cur_new_prod < beq.tail_next
            if (beq.tail_next.elected_votes > 0 || beq.last_producer_count == 3) {
               if (cur_old_prod != beq.tail) {
                  beq.tail_prev = beq.tail;
                  beq.tail = beq.tail_next;
               }
               beq.tail = beq.tail_next;
               refresh_backup_tail_next = true;
               if (cur_old_prod == meq.tail_next) {
                  meq.tail_next.clear();
                  refresh_main_tail_next = true;
               }
               // beq-: del cur prod from backup queue
               producer_change_helper::del(cur_name, backup_changes);
               // beq+: add beq.tail_next to backup queue
               producer_change_helper::add(beq.tail_next.name, beq.tail_next.authority, backup_changes);
            } else { // beq.tail_next.elected_votes <= 0 && beq.last_producer_count > 3
               if (cur_old_prod == beq.tail) {
                  beq.tail = beq.tail_prev;
                  refresh_backup_tail_prev = true;
               } else if (cur_old_prod == beq.tail_prev) {
                  refresh_backup_tail_prev = true;
               } else if (cur_old_prod == meq.tail_next) {
                  refresh_main_tail_next = true;
               }

               beq.last_producer_count--;
               // beq-: del cur prod from backup queue
               producer_change_helper::del(cur_name, backup_changes);
            }
         }
      } else { // cur_old_prod < beq.tail && cur_new_prod < meq.tail

         if (cur_new_prod >= beq.tail) {
            ASSERT(cur_new_prod != beq.tail)
            // beq+: add cur prod to backup queue
            producer_change_helper::add(cur_name, producer_authority, backup_changes);

            if ( beq.last_producer_count < ext.max_backup_producer_count &&
                 cur_old_prod != beq.tail_next &&
                 beq.tail.elected_votes > 0 )
            {
               beq.last_producer_count++;
               if (cur_new_prod < beq.tail_prev) {
                  beq.tail_prev = cur_new_prod;
               } else if (cur_new_prod > meq.tail_next) { // cur_new_prod > beq.tail_prev
                  meq.tail_next = cur_new_prod;
               }
               if (cur_old_prod == beq.tail_next) {
                  refresh_backup_tail_next = true;
               }
            } else {
               // beq-: pop beq.tail from backup queue
               producer_change_helper::del(beq.tail.name, backup_changes);
               beq.tail_next = beq.tail;
               if (cur_new_prod < beq.tail_prev) {
                  beq.tail = cur_new_prod;
               } else { // cur_new_prod > beq.tail_prev
                  beq.tail = beq.tail_prev;
                  refresh_backup_tail_prev = true;

                  if (cur_new_prod > meq.tail_next) { // cur_new_prod > beq.tail_prev
                     meq.tail_next = cur_new_prod;
                  }
               }
            }

         } else if (cur_new_prod >= beq.tail_next) { // cur_new_prod < beq.tail
            if ( beq.last_producer_count < ext.max_backup_producer_count &&
               cur_old_prod != beq.tail_next &&
               cur_new_prod.elected_votes > 0 )
            {
               // beq+: add cur prod to backup queue
               producer_change_helper::add(cur_name, producer_authority, backup_changes);
               beq.last_producer_count++;
               beq.tail_prev = beq.tail;
               beq.tail = cur_new_prod;
            } else { // beq.last_producer_count == ext.max_backup_producer_count || cur_old_prod == beq.tail_next || cur_new_prod.elected_votes <= 0
               beq.tail_next = cur_new_prod;
            }
         } else { // cur_new_prod < beq.tail_next
            if ( beq.last_producer_count < ext.max_backup_producer_count &&
               cur_old_prod != beq.tail_next &&
               beq.tail_next.elected_votes > 0 )
            {
               // beq+: add cur prod to backup queue
               producer_change_helper::add(cur_name, producer_authority, backup_changes);
               beq.last_producer_count++;
               beq.tail_prev = beq.tail;
               beq.tail = beq.tail_next;
               beq.tail_next = cur_new_prod;
            } else if (cur_old_prod == beq.tail_next) {
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

   }

} /// namespace eosiosystem
