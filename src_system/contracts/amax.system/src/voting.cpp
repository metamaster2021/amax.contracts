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
   using eosio::singleton;
   using eosio::producer_authority_add;
   using eosio::producer_authority_modify;
   using eosio::producer_authority_del;
   using eosio::producer_change_map;
   using eosio::print;
   using std::to_string;
   using std::string;

   static constexpr uint32_t min_backup_producer_count = 3;

   inline bool operator == ( const eosio::key_weight& lhs, const eosio::key_weight& rhs ) {
      return tie( lhs.key, lhs.weight ) == tie( rhs.key, rhs.weight );
   }

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
      auto find_pos(index_t &idx, const producer_elected_info &prod, const char* title) {
         auto itr = idx.lower_bound(producer_info::by_elected_prod(prod.name, prod.elected_votes, true));

         size_t steps = 1;
         while(true) {
            // if (steps >= 3) itr = idx.end();
            CHECK(itr != idx.end(), string(title) + ", pos not found! steps:" + to_string(steps) +
               " expectd:" + prod.name.to_string() + ":" + to_string(prod.elected_votes));
            if (itr->owner == prod.name) {
               #ifdef TRACE_PRODUCER_CHANGES
               print("found position of prod:", prod.name, ":", prod.elected_votes, " steps:", steps, "\n");
               #endif//TRACE_PRODUCER_CHANGES
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
      void fetch_prev(index_t &idx, const producer_elected_info &tail, producer_elected_info &prev, bool check_found, const char* title) {
         auto itr = find_pos(idx, tail, title);
         auto begin = idx.begin();
         check(begin != idx.end(), "electedprod index of producer table is empty");
         if (itr != begin) {
            itr--;
            prev = {itr->owner, itr->total_votes, itr->producer_authority};
            ASSERT(tail < prev);
            #ifdef TRACE_PRODUCER_CHANGES
            eosio::print(title, " updated: ", itr->owner, ":", itr->total_votes, "\n");
            #endif//TRACE_PRODUCER_CHANGES
         } else {
            if (check_found) {
               CHECK(false, string(title) + " not found! tail: " + tail.name.to_string() + ":" + to_string(tail.elected_votes))
            }
            #ifdef TRACE_PRODUCER_CHANGES
            eosio::print(title, " cleared\n");
            #endif//TRACE_PRODUCER_CHANGES
            prev.clear();
         }
      }

      template<typename index_t>
      void fetch_next(index_t &idx, const producer_elected_info &tail, producer_elected_info &next, bool check_found, const char* title) {
         auto itr = find_pos(idx, tail, title);
         itr++;
         if (itr != idx.end() && itr->ext) {
            next = {itr->owner, itr->total_votes, itr->producer_authority};
            ASSERT(next < tail);
            #ifdef TRACE_PRODUCER_CHANGES
            eosio::print(title, " updated: ", itr->owner, ":", itr->total_votes, "\n");
            #endif//TRACE_PRODUCER_CHANGES
         } else {
            if (check_found) {
               CHECK(false, string(title) + " not found! tail: " + tail.name.to_string() + ":" + to_string(tail.elected_votes))
            }
            #ifdef TRACE_PRODUCER_CHANGES
            eosio::print(title, " cleared\n");
            #endif//TRACE_PRODUCER_CHANGES
            next.clear();
         }
      }

   }

   void system_contract::initelects( const name& payer, uint32_t max_backup_producer_count ) {
      require_auth( payer );
      check(_gstate.thresh_activated_stake_time != time_point(),
         "cannot initelects until the chain is activated (at least 5% of all tokens participate in voting)");
      check(max_backup_producer_count >= min_backup_producer_count,
         "max_backup_producer_count must >= " + to_string(min_backup_producer_count));
      check(!_elect_gstate.is_init(), "elected producer has been initialized");

      auto block_time = current_block_time();

      _gstate.last_producer_schedule_update = block_time;
      eosio::proposed_producer_changes changes;
      changes.main_changes.clear_existed = true;
      changes.backup_changes.clear_existed = true;

      auto idx = _producers.get_index<"prototalvote"_n>();
      auto elect_idx = _producers.get_index<"electedprod"_n>();
      elect_global_state& egs = _elect_gstate;
      _elect_gstate.elected_version = 1;
      _elect_gstate.max_backup_producer_count = max_backup_producer_count;
      auto& meq = _elect_gstate.main_elected_queue;
      auto& beq = _elect_gstate.backup_elected_queue;
      auto &main_changes = changes.main_changes;
      auto &backup_changes = changes.backup_changes;

      check(_elected_changes.begin() == _elected_changes.end(), "elected change table is not empty" );

      // TODO: need using location to order producers?
      for( auto it = idx.cbegin(); it != idx.cend() && 0 < it->total_votes && it->active(); ++it ) {
         idx.modify( it, payer, [&]( auto& p ) {
            p.update_elected_votes(true);
            if (elect_idx.iterator_to(p) == elect_idx.end()) {
               elect_idx.emplace_index(p, payer);
            }
         });
         if (main_changes.changes.size() < _elect_gstate.max_main_producer_count) {
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
      uint32_t min_producer_count = _elect_gstate.max_main_producer_count + min_backup_producer_count + 1;

      CHECK(main_changes.producer_count + backup_changes.producer_count + 1 >= min_producer_count,
            "there must be at least " + to_string(min_producer_count) + " valid producers");

      auto ret = set_proposed_producers_ex( changes );
      CHECK(ret >= 0, "set proposed producers to native system failed(" + std::to_string(ret) + ")");
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
         auto elect_idx = _producers.get_index<"electedprod"_n>();
         auto elected_info_old = prod->get_elected_info();
         _producers.modify( prod, producer, [&]( producer_info& info ){
            info.producer_key       = producer_key;
            info.is_active          = true;
            info.url                = url;
            info.location           = location;
            info.producer_authority = producer_authority;
            info.update_elected_votes(true);
            if ( info.last_claimed_time == time_point() )
               info.last_claimed_time = ct;

            if (elect_idx.iterator_to(info) == elect_idx.end()) {
               elect_idx.emplace_index(info, producer);
            }
         });
         if (_elect_gstate.is_init()) {
            ASSERT(prod->ext && elect_idx.iterator_to(*prod) != elect_idx.end());
            proposed_producer_changes changes;
            process_elected_producer(elected_info_old, prod->get_elected_info(), changes);

            if ( !changes.backup_changes.changes.empty() || !changes.main_changes.changes.empty() ) {
               auto producer_change_id = ++_elect_gstate.last_producer_change_id;
               _elected_changes.emplace( producer, [&]( auto& c ) {
                     c.id        = producer_change_id;
                     c.changes   = changes;
               });
            }
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
            if (!proxy || proxy != voter->proxy ) {
               auto old_proxy = _voters.find( voter->proxy.value );
               check( old_proxy != _voters.end(), "old proxy not found" ); //data corruption
               _voters.modify( old_proxy, same_payer, [&]( auto& vp ) {
                     vp.proxied_vote_weight -= voter->last_vote_weight;
                  });
               propagate_weight_change( *old_proxy, voter_name );
            }
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
         double old_proxy_weight = 0;
         if (voter->proxy && proxy == voter->proxy) {
            old_proxy_weight = voter->last_vote_weight;
         }

         if ( new_vote_weight >= 0 || old_proxy_weight > 0 ) {
            _voters.modify( new_proxy, same_payer, [&]( auto& vp ) {
                  vp.proxied_vote_weight += new_vote_weight - old_proxy_weight;
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

      auto elect_idx = _producers.get_index<"electedprod"_n>();
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
            auto elected_info_old = pitr->get_elected_info();
            _producers.modify( pitr, same_payer, [&]( auto& p ) {
               p.total_votes += pd.second.first;
               if ( p.total_votes < 0 ) { // floating point arithmetics can give small negative numbers
                  p.total_votes = 0;
               }
               p.update_elected_votes();
               _gstate.total_producer_vote_weight += pd.second.first;
               //check( p.total_votes >= 0, "something bad happened" );
            });
            if (_elect_gstate.is_init() && pitr->ext) {
               ASSERT(elect_idx.iterator_to(*pitr) != elect_idx.end());
               process_elected_producer(elected_info_old, pitr->get_elected_info(), changes);
            }
         } else {
            if( pd.second.second ) {
               check( false, ( "producer " + pd.first.to_string() + " is not registered" ).data() );
            }
         }
      }

      if ( !changes.backup_changes.changes.empty() || !changes.main_changes.changes.empty() ) {
         auto producer_change_id = ++_elect_gstate.last_producer_change_id;
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

      auto elect_idx = _producers.get_index<"electedprod"_n>();

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
               auto elected_info_old = prod.get_elected_info();
               _producers.modify( prod, same_payer, [&]( auto& p ) {
                  p.total_votes += delta;
                  if ( p.total_votes < 0 ) { // floating point arithmetics can give small negative numbers
                     p.total_votes = 0;
                  }
                  p.update_elected_votes();
                  _gstate.total_producer_vote_weight += delta;
               });

               if (_elect_gstate.is_init() && prod.ext) {
                  ASSERT(elect_idx.iterator_to(prod) != elect_idx.end());
                  process_elected_producer(elected_info_old, prod.get_elected_info(), changes);
               }
            }

            if ( !changes.backup_changes.changes.empty() || !changes.main_changes.changes.empty() ) {
               auto producer_change_id = ++_elect_gstate.last_producer_change_id;
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

   void system_contract::process_elected_producer(const producer_elected_info& prod_old,
                           const producer_elected_info& prod_new, proposed_producer_changes &changes) {

      ASSERT(_elect_gstate.is_init())

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

            if (prod_new > beq.tail) {
               if (prod_new < beq.tail_prev) {
                  beq.tail_prev = prod_new;
               }
               // beq+: add cur prod to backup queue
               producer_change_helper::add(cur_name, prod_new.authority, backup_changes);
            } else if (prod_new > beq.tail_next) { // prod_new < beq.tail
               if (prod_new.elected_votes > 0 || beq.last_producer_count == 3) {
                  beq.tail_prev = beq.tail;
                  beq.tail = prod_new;
                  // beq+: add cur prod to backup queue
                  producer_change_helper::add(cur_name, prod_new.authority, backup_changes);
               } else { // prod_new.elected_votes <= 0 && beq.last_producer_count
                  beq.tail_next = prod_new;
                  beq.last_producer_count--;
               }
            } else { // prod_new < beq.tail_next
               if(beq.tail_next.elected_votes > 0 || beq.last_producer_count == 3) {
                  beq.tail_prev = beq.tail;
                  beq.tail = beq.tail_next;
                  beq.tail_next.clear();
                  refresh_backup_tail_next = true;
               } else { // prod_new < beq.tail_next
                  beq.last_producer_count--;
               }
            }
            meq.tail_next.clear();
            refresh_main_tail_next = true;
         }

         if (modify_only && prod_new.authority != prod_old.authority) {
            // meq*: modify cur prod in meq
            producer_change_helper::add(cur_name, prod_new.authority, main_changes);
         }
      } else if (prod_new > meq.tail) { // prod_old < meq.tail

         // meq-: del meq.tail from main queue
         producer_change_helper::del(meq.tail.name, main_changes);
         // meq+: add cur prod to main queue
         producer_change_helper::add(cur_name, prod_new.authority, main_changes);
         // beq+: add meq.tail to backup queue
         producer_change_helper::add(meq.tail.name, meq.tail.authority, backup_changes);

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
            producer_change_helper::del(cur_name, backup_changes);
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
               producer_change_helper::del(beq.tail.name, backup_changes);
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
            if (prod_new.elected_votes > 0 || beq.last_producer_count == 3) {
               if (prod_old != beq.tail) {
                  beq.tail_prev = beq.tail;
                  if (prod_old == meq.tail_next) {
                     refresh_main_tail_next = true;
                  }
               }
               beq.tail = prod_new;
            } else { // prod_new.elected_votes <= 0 && beq.last_producer_count > 3
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
               producer_change_helper::del(cur_name, backup_changes);
               modify_only = false;
            }
         } else { // prod_new < beq.tail_next
            modify_only = false;
            if (beq.tail_next.elected_votes > 0 || beq.last_producer_count == 3) {
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
               producer_change_helper::del(cur_name, backup_changes);
               // beq+: add beq.tail_next to backup queue
               producer_change_helper::add(beq.tail_next.name, beq.tail_next.authority, backup_changes);
            } else { // beq.tail_next.elected_votes <= 0 && beq.last_producer_count > 3
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
               producer_change_helper::del(cur_name, backup_changes);
            }
         }
         if (modify_only && prod_new.authority != prod_old.authority) {
            // meq*: modify cur prod in beq
            producer_change_helper::modify(cur_name, prod_new.authority, backup_changes);
         }
      } else { // prod_old < beq.tail && prod_new < meq.tail

         if (prod_new >= beq.tail) {
            ASSERT(prod_new != beq.tail)
            // beq+: add cur prod to backup queue
            producer_change_helper::add(cur_name, prod_new.authority, backup_changes);

            if ( beq.last_producer_count < _elect_gstate.max_backup_producer_count &&
                 prod_old != beq.tail_next &&
                 beq.tail.elected_votes > 0 )
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
               producer_change_helper::del(beq.tail.name, backup_changes);
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
            if ( beq.last_producer_count < _elect_gstate.max_backup_producer_count &&
               prod_old != beq.tail_next &&
               prod_new.elected_votes > 0 )
            {
               // beq+: add cur prod to backup queue
               producer_change_helper::add(cur_name, prod_new.authority, backup_changes);
               beq.last_producer_count++;
               beq.tail_prev = beq.tail;
               beq.tail = prod_new;
            } else { // beq.last_producer_count == ext.max_backup_producer_count || prod_old == beq.tail_next || prod_new.elected_votes <= 0
               beq.tail_next = prod_new;
            }
         } else { // prod_new < beq.tail_next
            if ( beq.last_producer_count < _elect_gstate.max_backup_producer_count &&
               prod_old != beq.tail_next &&
               beq.tail_next.elected_votes > 0 )
            {
               // beq+: add beq.tail_next to backup queue
               producer_change_helper::add(beq.tail_next.name, beq.tail_next.authority, backup_changes);
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

} /// namespace eosiosystem
