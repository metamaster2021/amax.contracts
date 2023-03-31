#include <Runtime/Runtime.h>
#include <boost/test/unit_test.hpp>
#include <cstdlib>
#include <eosio/chain/contract_table_objects.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/global_property_object.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/chain/wast_to_wasm.hpp>
#include <eosio/chain/config.hpp>
#include <eosio/chain/fork_database.hpp>
#include <fc/log/logger.hpp>
#include <iostream>
#include <sstream>

// make sure eosio_system_tester derive from backup_block_tester
// #define TESTER test1::backup_block_tester
#define TESTER backup_block_tester

#include "amax.system_tester.hpp"

#define int128_t  eosio::chain::int128_t
#define uint128_t eosio::chain::uint128_t

#define LESS(a, b)                     (a) < (b) ? true : false
#define LARGER(a, b)                   (a) > (b) ? true : false
#define LESS_OR(a, b, other_compare)   (a) < (b) ? true : (a) > (b) ? false : ( other_compare )
#define LARGER_OR(a, b, other_compare) (a) > (b) ? true : (a) < (b) ? false : ( other_compare )

#define CHECK(exp, msg) { if (exp) { BOOST_REQUIRE_MESSAGE( (exp), msg ); } }

static constexpr int128_t  HIGH_PRECISION    = 1'000'000'000'000'000'000; // 10^18

static const fc::microseconds block_interval_us = fc::microseconds(eosio::chain::config::block_interval_us);

uint64_t sum_consecutive_int(uint64_t max_num) {
   uint64_t sum = 0;
   for (size_t i = 1; i <= max_num; i++){
      sum += i;
   }
   return sum;
}

inline float128_t operator+(const float128_t& a, const float128_t& b) {
   return f128_add(a, b);
}
inline float128_t operator-(const float128_t& a, const float128_t& b) {
   return f128_sub(a, b);
}
inline float128_t operator*(const float128_t& a, const float128_t& b) {
   return f128_mul(a, b);
}

inline float128_t operator/(const float128_t& a, const float128_t& b) {
   return f128_div(a, b);
}

inline float128_t to_softfloat128( double d ) {
   return f64_to_f128(to_softfloat64(d));
}

 inline uint128_t by_elected_prod(const name& owner, int64_t elected_votes, bool is_active = true) {
   static constexpr int64_t int64_max = std::numeric_limits<int64_t>::max();
   static constexpr int64_t uint64_max = std::numeric_limits<uint64_t>::max();
   static_assert( uint64_max - (uint64_t)int64_max == (uint64_t)int64_max + 1 );
   BOOST_ASSERT(elected_votes >= 0 && elected_votes != int64_max);
   uint64_t hi = is_active ? int64_max - elected_votes : uint64_max - elected_votes;
   return uint128_t(hi) << 64 | owner.to_uint64_t();
}

struct producer_elected_info {
   name                    name;
   int64_t                 elected_votes = 0.0;
   block_signing_authority authority;
   bool empty() const {
      return !bool(name);
   }

};
FC_REFLECT( producer_elected_info, (name)(elected_votes)(authority) )


inline uint128_t by_elected_prod(const producer_elected_info& v) {
   return by_elected_prod(v.name, v.elected_votes);
}

inline bool operator<(const producer_elected_info& a, const producer_elected_info& b)  {
   return LESS_OR(a.elected_votes, b.elected_votes, LARGER(a.name, b.name));
}

inline bool operator>(const producer_elected_info& a, const producer_elected_info& b)  {
   return LARGER_OR(a.elected_votes, b.elected_votes, LESS(a.name, b.name));
}

inline bool operator<=(const producer_elected_info& a, const producer_elected_info& b)  {
      return !(a > b);
}
inline bool operator>=(const producer_elected_info& a, const producer_elected_info& b)  {
   return !(a < b);
}
inline bool operator==(const producer_elected_info& a, const producer_elected_info& b)  {
   return a.elected_votes == b.elected_votes && a.name == b.name;
}
inline bool operator!=(const producer_elected_info& a, const producer_elected_info& b)  {
   return !(a == b);
}

struct producer_elected_queue {
   uint32_t                  last_producer_count = 0;
   producer_elected_info    tail;
   producer_elected_info    tail_prev;
   producer_elected_info    tail_next;
};
FC_REFLECT( producer_elected_queue, (last_producer_count)(tail)(tail_prev)(tail_next) )

struct elect_global_state {
   uint8_t                    elected_version               = 0;
   int128_t                   total_producer_elected_votes  = 0; /// the sum of all producer elected votes
   uint32_t                   max_main_producer_count       = 21;
   uint32_t                   max_backup_producer_count     = 10000;
   int64_t                    min_producer_votes            = 1'000'0000'0000;

   uint64_t                   last_producer_change_id       = 0;
   bool                       producer_change_interrupted   = false;
   producer_elected_queue     main_elected_queue;
   producer_elected_queue     backup_elected_queue;

   bool is_init() const  { return elected_version > 0; }
};

FC_REFLECT( elect_global_state, (elected_version)(total_producer_elected_votes)
                                (max_main_producer_count)(max_backup_producer_count)(min_producer_votes)
                                (last_producer_change_id)(producer_change_interrupted)
                                (main_elected_queue)(backup_elected_queue) )

struct amax_global_state: public eosio::chain::chain_config {
   uint64_t free_ram()const { return max_ram_size - total_ram_bytes_reserved; }

   symbol               core_symbol;
   uint64_t             max_ram_size = 64ll*1024 * 1024 * 1024;
   uint64_t             total_ram_bytes_reserved = 0;
   int64_t              total_ram_stake = 0;

   block_timestamp_type      last_producer_schedule_update;
   int64_t              total_activated_stake = 0;
   time_point           thresh_activated_stake_time;
   uint16_t             last_producer_schedule_size = 0;
   double               total_producer_vote_weight = 0; /// the sum of all producer votes
   block_timestamp_type      last_name_close;

   uint16_t          new_ram_per_block = 0;
   block_timestamp_type   last_ram_increase;
   time_point        inflation_start_time;         // inflation start time
   asset             initial_inflation_per_block;  // initial inflation per block
   uint64_t          reverved = 0;                 // reverved
   uint8_t           revision = 0; ///< used to track version updates in the future.
};

FC_REFLECT_DERIVED( amax_global_state, (eosio::chain::chain_config),
                  (core_symbol)(max_ram_size)(total_ram_bytes_reserved)(total_ram_stake)
                  (last_producer_schedule_update)
                  (total_activated_stake)(thresh_activated_stake_time)
                  (last_producer_schedule_size)(total_producer_vote_weight)(last_name_close)
                  (new_ram_per_block)(last_ram_increase)
                  (inflation_start_time)(initial_inflation_per_block)(reverved)
                  (revision)
)

struct producer_info_ext {
   int64_t        elected_votes     = 0;
   uint32_t       reward_shared_ratio = 0;
};
FC_REFLECT( producer_info_ext, (elected_votes)(reward_shared_ratio))

struct producer_info {
   name                                                     owner;
   double                                                   total_votes = 0;
   public_key_type                                          producer_key; /// a packed public key object
   bool                                                     is_active = true;
   std::string                                              url;
   uint16_t                                                 location = 0;
   time_point                                               last_claimed_time;
   asset                                                    unclaimed_rewards;
   block_signing_authority                                  producer_authority;
   eosio::chain::may_not_exist<producer_info_ext>           ext;
};

FC_REFLECT( producer_info, (owner)(total_votes)(producer_key)(is_active)(url)(location)
                                    (last_claimed_time)(unclaimed_rewards)(producer_authority)(ext) )

struct elected_change {
   uint64_t                      id;             // pk, auto increasement
   proposed_producer_changes     changes;
   block_timestamp_type          created_at;
};

FC_REFLECT( elected_change, (id)(changes)(created_at) )

vector<account_name> gen_producer_names(uint32_t count, uint64_t from) {
   vector<account_name> result;
   from >>= 4;
   for (uint64_t n = from; result.size() < count; n++) {
      result.emplace_back(n << 4);
   }
   return result;
}

struct producer_shared_reward {
   name                    owner;
   asset                   unallocated_rewards;
   asset                   allocating_rewards;
   asset                   allocated_rewards;
   int64_t                 votes                = 0;
   int128_t                rewards_per_vote     = 0;
   block_timestamp_type    update_at;
};

FC_REFLECT( producer_shared_reward, (owner)(unallocated_rewards)(allocating_rewards)(allocated_rewards)
                                    (votes)(rewards_per_vote)(update_at) )

struct vote_reward_info {
   int128_t                last_rewards_per_vote = 0;
};
FC_REFLECT( vote_reward_info, (last_rewards_per_vote) )

struct voter_reward {
   name                             owner;
   int64_t                          votes;
   std::map<name, vote_reward_info> producers;
   asset                            unclaimed_rewards;
   asset                            claimed_rewards;
   block_timestamp_type             update_at;
};

FC_REFLECT( voter_reward, (owner)(votes)(producers)(unclaimed_rewards)(claimed_rewards)(update_at) )

namespace producer_change_helper {

   void merge(const producer_change_map& change_map, flat_map<name, block_signing_authority> &producers, const std::string& title) {
      if (change_map.clear_existed) BOOST_FAIL(title + ": clear_existed can not be true"
                           + ", producer_count=" + std::to_string(change_map.producer_count));
      for (const auto& change : change_map.changes) {
         const auto &producer_name = change.first;
         change.second.visit( [&](const auto& c ) {
            switch(c.change_operation) {
               case producer_change_operation::add:
                  if (producers.count(producer_name) > 0) {
                     BOOST_FAIL(title + ": added producer is existed: " + producer_name.to_string()
                           + ", producer_count=" + std::to_string(change_map.producer_count));
                  }
                  if (!c.authority) {
                     BOOST_FAIL(title + ": added producer authority can not be empty: " + producer_name.to_string()
                           + ", producer_count=" + std::to_string(change_map.producer_count));
                  }
                  producers[producer_name] = *c.authority;
               case producer_change_operation::modify:
                  if (producers.count(producer_name) == 0) {
                     BOOST_FAIL(title + ": modifed producer is not existed: " + producer_name.to_string()
                           + ", producer_count=" + std::to_string(change_map.producer_count));
                  }
                  if (!c.authority) {
                     BOOST_FAIL(title + ": modified producer authority can not be empty: " + producer_name.to_string()
                           + ", producer_count=" + std::to_string(change_map.producer_count));
                  }
                  producers[producer_name] = *c.authority;
                  break;
               case producer_change_operation::del:
                  if (producers.count(producer_name) == 0) {
                     BOOST_FAIL(title + ": modifed producer is not existed: " + producer_name.to_string()
                           + ", producer_count=" + std::to_string(change_map.producer_count));
                  }
                  producers.erase(producer_name);
                  break;
            }
         });
      }
      if (producers.size() != change_map.producer_count ) {
         BOOST_FAIL(title + ": the merged producer count: " + std::to_string(producers.size())
            + " mismatch with expected: " + std::to_string(change_map.producer_count)
            + ", producer_count=" + std::to_string(change_map.producer_count));
      }
   }

   void merge(const elected_change& src_change, flat_map<name, block_signing_authority> &main_producers,
                     flat_map<name, block_signing_authority> &backup_producers, const string& title) {

      merge(src_change.changes.main_changes, main_producers, title + " main producers");
      merge(src_change.changes.backup_changes, backup_producers, title + " backup producers");

      auto mitr = main_producers.begin();
      auto bitr = backup_producers.begin();
      while (mitr != main_producers.end() && bitr != backup_producers.end()) {
         if (mitr->first == bitr->first) {
            BOOST_FAIL(title + " producer: " + mitr->first.to_string()
               + " can not be in both main and backup producer"
               + ", main_producer_count=" + std::to_string(src_change.changes.main_changes.producer_count)
               + ", backup_producer_count=" + std::to_string(src_change.changes.backup_changes.producer_count));
         } else if (mitr->first < bitr->first){
            mitr++;
         } else {
            bitr++;
         }
      }

   }

   void merge(const vector<elected_change>& src_changes, flat_map<name, block_signing_authority> &main_producers,
                     flat_map<name, block_signing_authority> &backup_producers) {
      for (size_t i = 0; i < src_changes.size(); i++) {
         merge(src_changes[i], main_producers, backup_producers, "[" + std::to_string(i) + "]");
      }
   }

   flat_map<name, block_signing_authority> producers_from(const producer_authority_schedule &schedule) {
      flat_map<name, block_signing_authority> ret;
      const auto& producers = schedule.producers;
      for (size_t i = 0; i < producers.size(); i++) {
         const auto& p = producers[i];
         if (i > 0) {
            if (p.producer_name <= producers[i - 1].producer_name) {
               BOOST_FAIL("producers not sorted by name with ascending order");
            }
         }
         ret[p.producer_name] = p.authority;
      }
      return ret;
   }

};

struct voter_info_t {
   vector<name> producers;
   asset staked               = CORE_ASSET(0);
   asset net                  = CORE_ASSET(0);
   asset cpu                  = CORE_ASSET(0);
   int64_t last_votes         = 0; /// the vote weight cast the last time the vote was updated
};

FC_REFLECT( voter_info_t, (producers)(staked)(net)(cpu)(last_votes) )

using namespace eosio_system;

struct producer_change_tester : eosio_system_tester {

   vector<account_name> producers = gen_producer_names(100, N(prod.1111111).to_uint64_t());
   vector<account_name> voters = gen_producer_names(100, N(voter.111111).to_uint64_t());
   fc::flat_map<name, producer_elected_info> producer_map;
   fc::flat_map<name, voter_info_t> voter_map;


   // push action without commit current block
   transaction_trace_ptr push_action( const account_name& signer, const action_name &name, const variant_object &data ) {
      return base_tester::push_action( config::system_account_name, name, signer, data);
   }

   // void initelects( uint32_t max_backup_producer_count );
   transaction_trace_ptr initelects(uint32_t max_backup_producer_count) {
      // push action without commit current block
      return push_action(config::system_account_name, N(initelects), mvo()
                  ("max_backup_producer_count", max_backup_producer_count)
      );
   }

   transaction_trace_ptr setinflation(const time_point& inflation_start_time, const asset& initial_inflation_per_block ) {
      // push action without commit current block
      return push_action(config::system_account_name, N(setinflation), mvo()
                  ("inflation_start_time", inflation_start_time)
                  ("initial_inflation_per_block", initial_inflation_per_block)
      );
   }

   template<typename T>
   T get_row_by_account(const name& code, const name& scope, const name& table, const account_name& act ) const {
      vector<char> data = base_tester::get_row_by_account( code, scope, table, act );
      if (!data.empty()) {
         return fc::raw::unpack<T>(data);
      }
      return T();
   }

   amax_global_state get_global_state() {
      return get_row_by_account<amax_global_state>( config::system_account_name, config::system_account_name,
         N(global), N(global) );
   }

   elect_global_state get_elect_global_state() {
      return get_row_by_account<elect_global_state>( config::system_account_name, config::system_account_name,
         N(electglobal), N(electglobal) );
   }

   producer_info get_producer_info(const name& producer_name) {
      return get_row_by_account<producer_info>( config::system_account_name, config::system_account_name,
         N(producers), producer_name );
   }

   producer_shared_reward get_producer_shared_reward(const name& producer_name) {
      return get_row_by_account<producer_shared_reward>( N(amax.reward), N(amax.reward),
         N(producers), producer_name );
   }

   voter_reward get_voter_reward_info(const name& owner) {
      return get_row_by_account<voter_reward>( N(amax.reward), N(amax.reward),
         N(voters), owner );
   }

   static auto get_producer_private_key( name producer_name, uint64_t version = 1 ) {
      return get_private_key(producer_name, std::to_string(version));
   }

   static auto get_producer_public_key( name producer_name, uint64_t version = 1 ) {
      return get_producer_private_key(producer_name, version).get_public_key();
   }

   block_signing_authority make_producer_authority(name producer_name, uint64_t version = 1){
      auto privkey = get_producer_private_key(producer_name, version);
      auto pubkey = privkey.get_public_key();
      add_block_signing_key(pubkey, privkey);
      return block_signing_authority_v0{
         1, {
            {pubkey, 1}
         }
      };
   }

   auto regproducer( const account_name& acnt, uint64_t version = 1 ) {
      return push_action( acnt, N(regproducer), mvo()
                          ("producer",  acnt )
                          ("producer_key", get_producer_public_key( acnt, version ) )
                          ("url", "" )
                          ("location", 0 )
                          ("reward_shared_ratio", 8000 )
      );
   }

   auto stake( const account_name& from, const account_name& to, const asset& net, const asset& cpu ) {
      return push_action( name(from), N(delegatebw), mvo()
                          ("from",     from)
                          ("receiver", to)
                          ("stake_net_quantity", net)
                          ("stake_cpu_quantity", cpu)
                          ("transfer", 0 )
      );
   }

   auto stake( const account_name& acnt, const asset& net, const asset& cpu ) {
      return stake( acnt, acnt, net, cpu );
   }

   auto vote( const account_name& voter, const std::vector<account_name>& producers, const account_name& proxy = name(0) ) {
      return push_action(voter, N(voteproducer), mvo()
                         ("voter",     voter)
                         ("proxy",     proxy)
                         ("producers", producers));
   }

   auto transfer( const name& from, const name& to, const asset& amount, const string& memo = "" ) {
      return base_tester::push_action( N(amax.token), N(transfer), from, mutable_variant_object()
                                ("from",        from)
                                ("to",          to )
                                ("quantity",    amount)
                                ("memo",        memo)
                                );
   }

   auto producer_claimrewards( const name& owner ) {
      return push_action(owner, N(claimrewards), mvo()
                         ("owner",     owner));
   }

   auto voter_claimrewards( const name& voter_name ) {
      return base_tester::push_action( N(amax.reward), N(claimrewards), voter_name, mutable_variant_object()
                                ("voter_name",       voter_name) );
   }


   vector<producer_elected_info> get_elected_producers(fc::flat_map<name, producer_elected_info> &producer_map,
                     size_t max_size = -1) {
      vector<producer_elected_info> ret;
      ret.reserve(producer_map.size());
      for (const auto& p : producer_map) {
         ret.push_back(p.second);
      }
      std::sort( ret.begin(), ret.end(), []( const producer_elected_info& a, const producer_elected_info& b ) {
         return a > b;
      } );

      auto sz = std::min(max_size, producer_map.size());
      return vector<producer_elected_info>(ret.begin(), ret.begin() + sz);
   }

   const table_id_object* find_table_index_id( const name& code, const name& scope, const name& table, uint64_t index_pos ) {
      EOS_ASSERT(index_pos <= 0x000000000000000FULL, eosio::chain::contract_table_query_exception, "table index pos too large");
      auto table_with_index = name(table.to_uint64_t() | index_pos);
      return control->db().find<table_id_object, by_code_scope_table>(boost::make_tuple(code, scope, table_with_index));
   }

   const table_id_object& get_table_index_id( const name& code, const name& scope, const name& table, uint64_t index_pos ) {
      const table_id_object* t_id = find_table_index_id(code, scope, table, index_pos);
      EOS_ASSERT(t_id != nullptr, eosio::chain::contract_table_query_exception, "table index id not found");
      return *t_id;
   }

   const table_id_object& get_table_id( const name& code, const name& scope, const name& table ) {
      const table_id_object* idx_id = find_table(code, scope, table);
      EOS_ASSERT(idx_id != nullptr, eosio::chain::contract_table_query_exception, "table id not found");
      return *idx_id;
   }

   vector<producer_elected_info> get_elected_producers_from_db(uint8_t min_elected_version, size_t max_size = -1) {
      vector<producer_elected_info> ret;
      const auto& db = control->db();
      const auto& idx_id = get_table_index_id(config::system_account_name, config::system_account_name, N(producers), 1);
      const auto& t_id = get_table_id(config::system_account_name, config::system_account_name, N(producers) );
      const auto& idx = db.get_index<eosio::chain::index128_index, eosio::chain::by_secondary>();

      uint128_t lowest = eosio::chain::secondary_key_traits<uint128_t>::true_lowest();
      vector<char> data;
      auto itr = idx.lower_bound( boost::make_tuple( idx_id.id, lowest, 0 ) );
      for (; itr != idx.end() && itr->t_id == idx_id.id; itr++) {
         const auto* itr2 = db.find<eosio::chain::key_value_object, eosio::chain::by_scope_primary>( boost::make_tuple(t_id.id, itr->primary_key) );
         EOS_ASSERT(itr2 != nullptr, eosio::chain::contract_table_query_exception,
                  "primary data not found by key:" + std::to_string(itr->primary_key));
         data.resize( itr2->value.size() );
         memcpy( data.data(), itr2->value.data(), itr2->value.size() );
         producer_info info = fc::raw::unpack<producer_info>(data);
         // if (info.ext.value.elected_votes <= 0) {
         //    wdump( (info.owner) (info.ext.value.elected_votes));
         //    break;
         // }
         ret.push_back({info.owner, info.ext.value.elected_votes, info.producer_authority});
      }
      auto sz = std::min(max_size, ret.size());
      return vector<producer_elected_info>(ret.begin(), ret.begin() + sz);
   }

   vector<elected_change> get_elected_change_from_db() {
      static const name table_name = N(electchange);
      const auto& db = control->db();
      // const auto idx_id = find_table_index_id(config::system_account_name, config::system_account_name, table_name, 0);
      const auto t_id = find_table(config::system_account_name, config::system_account_name, table_name );
      vector<elected_change> rows;
      vector<char> data;
      if (t_id != nullptr) {
         const auto& idx = db.get_index<eosio::chain::key_value_index, eosio::chain::by_scope_primary>();

         auto itr = idx.lower_bound( boost::make_tuple( t_id->id, 0 ) );

         for (; itr != idx.end() && itr->t_id == t_id->id; itr++) {
            data.resize( itr->value.size() );
            memcpy( data.data(), itr->value.data(), itr->value.size() );
            auto changes = fc::raw::unpack<elected_change>(data);
            rows.push_back(std::move(changes));

         }
      }
      return rows;
   }

   size_t elected_change_count_in_db() {
      return get_elected_change_from_db().size();
   }

   bool elected_change_empty_in_db() {
      static const name table_name = N(electchange);
      const auto& db = control->db();
      // const auto& idx_id = get_table_index_id(config::system_account_name, config::system_account_name, table_name, 0);
      const auto& t_id = find_table(config::system_account_name, config::system_account_name, table_name );
      const auto& idx = db.get_index<eosio::chain::key_value_index, eosio::chain::by_scope_primary>();
      if (t_id != nullptr) {
         auto itr = idx.lower_bound( boost::make_tuple( t_id->id, 0 ) );
         return itr == idx.end();
      }
      return true;
   }

   void get_producer_schedule(const vector<producer_elected_info>& elected_producers, uint32_t main_producer_count,
            uint32_t backup_producer_count, flat_map<name, block_signing_authority> &main_schedule,
            flat_map<name, block_signing_authority> &backup_schedule) {
      main_schedule.clear();
      backup_schedule.clear();
      for (size_t i = 0; i < main_producer_count; i++) {
         main_schedule.emplace(elected_producers[i].name, elected_producers[i].authority);
      }
      for (size_t i = main_producer_count; i < main_producer_count + backup_producer_count; i++) {
         backup_schedule.emplace(elected_producers[i].name, elected_producers[i].authority);
      }
   }

   std::map<name, int64_t> get_voters_of_producer(const name& producer_name) {
      std::map<name, int64_t> ret;
      for(const auto& v : voter_map) {
         const auto& producers = v.second.producers;
         if ( std::find(producers.begin(), producers.end(), producer_name) != producers.end() ) {
            ret[v.first] = v.second.last_votes;
            continue;
         }
      }
      return ret;
   }

   producer_authority calc_main_scheduled_producer( const vector<producer_authority> &producers, block_timestamp_type t ) {
      auto index = t.slot % (producers.size() * config::producer_repetitions);
      index /= config::producer_repetitions;
      return producers[index];
   }

   optional<producer_authority> calc_backup_scheduled_producer( const backup_producer_schedule_ptr &schedule, block_timestamp_type t ) const {
      optional<producer_authority> result;
      if (schedule && schedule->producers.size() > 0) {
         auto index = t.slot % (schedule->producers.size() * config::backup_producer_repetitions);
         index /= config::backup_producer_repetitions;
         const auto& itr = schedule->producers.nth(index);
         return producer_authority{itr->first, itr->second};
      }
      return optional<producer_authority>{};
   }

   template<typename T>
   inline bool vector_matched( const std::vector<T>& a, const std::vector<T>& b, size_t sz ) {
      wdump( (a.size() )  (b.size()) (sz));
      if (a.size() < sz || b.size() < sz) {
         if (a.size() != b.size()) {
            return false;
         }
         sz = a.size();
      }
      for (size_t i = 0; i < sz; i++) {
         if (a[i] != b[i]) {
            wdump( (i) (a[i])(b[i]) );
            return false;
         }
      }
      return true;
   }

   template<typename Lambda>
   bool produce_blocks_until( size_t max_blocks, Lambda&& condition ) {
      for (size_t i = 0; i < max_blocks; i++) {
         produce_block();
         if (condition()) {
            return true;
         }
      }
      return false;
   }


   inline int128_t calc_rewards_per_vote(const int128_t& old_rewards_per_vote, int64_t rewards, int64_t votes) {
      auto  new_rewards_per_vote = old_rewards_per_vote + rewards * HIGH_PRECISION / votes;
      CHECK(new_rewards_per_vote >= old_rewards_per_vote, "calculated rewards_per_vote overflow");
      return new_rewards_per_vote;
   }

   inline int64_t calc_voter_rewards(int64_t votes, const int128_t& rewards_per_vote) {
      // with rounding-off method
      int128_t rewards = votes * rewards_per_vote / (HIGH_PRECISION);
      CHECK(votes >= 0, "calculated rewards can not be negative");
      CHECK(rewards >= 0 && rewards <= std::numeric_limits<int64_t>::max(),
            "calculated rewards overflow");
      return rewards;
   }

};

template <typename A, typename B, typename D>
bool near(A a, B b, D delta) {
   if (abs(a - b) <= delta)
      return true;
   elog("near: ${a} ${b}", ("a", a)("b", b));
   return false;
}

BOOST_AUTO_TEST_SUITE(producer_change_tests)

BOOST_FIXTURE_TEST_CASE(producer_elects_test, producer_change_tester) try {
   produce_block();

   produce_block();
   auto ram_asset = core_sym::from_string("10000.0000");
   for (size_t i = 0; i < producers.size(); i++) {
      create_account_with_resources( producers[i], config::system_account_name, 10 * 1024 );
      regproducer( producers[i] );
      producer_map[ producers[i] ] = { producers[i], 0, make_producer_authority(producers[i], 1) };
      // wdump( (producers[i]) (get_producer_info(producers[i])) );

      if (i % 20 == 0)
         produce_block();
   }

   auto total_staked = core_sym::min_activated_stake + core_sym::from_string("1.0000");
   uint64_t total_weight = sum_consecutive_int(voters.size());
   for (size_t i = 0; i < voters.size(); i++) {
      auto const& voter = voters[i];
      auto& voter_info = voter_map[voter];
      voter_info.staked = CORE_ASSET( total_staked.get_amount() * (i + 1) / total_weight );
      voter_info.net = CORE_ASSET(voter_info.staked.get_amount() / 2);
      voter_info.cpu = voter_info.staked - voter_info.net;
      voter_info.last_votes = voter_info.staked.get_amount();
      create_account_with_resources( voter, config::system_account_name, 10 * 1024);
      transfer(N(amax), voter, voter_info.staked);
      if(!stake( voter, voter_info.net, voter_info.cpu ) ) {
         BOOST_FAIL("stake failed");
      }
      size_t max_count = std::min(30ul, producers.size());
      voter_info.producers.resize(max_count);
      for (size_t j = 0; j < max_count; j++) {
         const auto& prod = producers[ (i + j) % producers.size() ];
         voter_info.producers[j] = prod;
         producer_map[prod].elected_votes += voter_info.last_votes;
      }
      std::sort( voter_info.producers.begin(), voter_info.producers.end());

      if( !vote( voter, voter_info.producers ) ) {
         BOOST_FAIL("vote failed");
      }

      if (i != voters.size() - 1 && i % 20 == 0)
         produce_block();
   }

   const auto& gpo = control->get_global_properties();

   auto gstate = get_global_state();
   auto elect_gstate = get_elect_global_state();
   BOOST_REQUIRE_EQUAL( elect_gstate.elected_version, 0 );

   wdump( (gstate.thresh_activated_stake_time) );
   BOOST_REQUIRE( gstate.thresh_activated_stake_time == control->pending_block_time() );

   initelects(43);
   elect_gstate = get_elect_global_state();
   BOOST_REQUIRE_EQUAL( elect_gstate.elected_version, 1 );
   BOOST_REQUIRE(gpo.proposed_schedule_block_num);
   BOOST_REQUIRE_EQUAL(*gpo.proposed_schedule_block_num, control->head_block_num() + 1);
   BOOST_REQUIRE_EQUAL(gpo.proposed_schedule.version, 0);
   BOOST_REQUIRE_EQUAL(gpo.proposed_schedule.producers.size(), 0);
   BOOST_REQUIRE_EQUAL(gpo.proposed_schedule_change.version, 1);
   BOOST_REQUIRE_EQUAL(gpo.proposed_schedule_change.main_changes.producer_count, 21);
   BOOST_REQUIRE_EQUAL(gpo.proposed_schedule_change.backup_changes.producer_count, 3);

   produce_block();

   elect_gstate = get_elect_global_state();
   // wdump( (elect_gstate) );
   BOOST_REQUIRE_EQUAL(elect_gstate.elected_version, 1);
   BOOST_REQUIRE_EQUAL(elect_gstate.max_main_producer_count, 21);
   BOOST_REQUIRE_EQUAL(elect_gstate.max_backup_producer_count, 43);
   BOOST_REQUIRE_EQUAL(elect_gstate.main_elected_queue.last_producer_count, 21);
   BOOST_REQUIRE_EQUAL(elect_gstate.backup_elected_queue.last_producer_count, 3);

   auto elected_producers = get_elected_producers(producer_map);
   auto elected_producers_in_db = get_elected_producers_from_db(elect_gstate.elected_version);
   // wdump(  (elected_producers) );
   // wdump(  (elected_producers_in_db) );
   BOOST_REQUIRE( vector_matched(elected_producers, elected_producers_in_db, 21) );

   // wdump( (elect_gstate.main_elected_queue.tail_prev)(elected_producers[19]) );
   BOOST_REQUIRE(elect_gstate.main_elected_queue.tail_prev     == elected_producers[19]);
   BOOST_REQUIRE(elect_gstate.main_elected_queue.tail          == elected_producers[20]);
   BOOST_REQUIRE(elect_gstate.main_elected_queue.tail_next     == elected_producers[21]);
   BOOST_REQUIRE(elect_gstate.backup_elected_queue.tail_prev   == elected_producers[22]);
   BOOST_REQUIRE(elect_gstate.backup_elected_queue.tail        == elected_producers[23]);
   BOOST_REQUIRE(elect_gstate.backup_elected_queue.tail_next   == elected_producers[24]);

   produce_block();
   auto hbs = control->head_block_state();
   auto header_exts = hbs->header_exts;
   // wdump( (hbs));

   BOOST_REQUIRE(!gpo.proposed_schedule_block_num);
   BOOST_REQUIRE_EQUAL( header_exts.count(producer_schedule_change_extension_v2::extension_id()) , 1 );
   const auto& new_producer_schedule = header_exts.lower_bound(producer_schedule_change_extension_v2::extension_id())->second.get<producer_schedule_change_extension_v2>();

   // wdump((new_producer_schedule));
   BOOST_REQUIRE_EQUAL(new_producer_schedule.version, 1);
   BOOST_REQUIRE_EQUAL(new_producer_schedule.main_changes.producer_count, 21);
   BOOST_REQUIRE_EQUAL(new_producer_schedule.backup_changes.producer_count, 3);

   BOOST_REQUIRE_EQUAL( hbs->pending_schedule.schedule_lib_num, control->head_block_num() );
   BOOST_REQUIRE( hbs->pending_schedule.schedule.contains<producer_schedule_change>());
   const auto& change = hbs->pending_schedule.schedule.get<producer_schedule_change>();
   BOOST_REQUIRE_EQUAL( change.version, 1 );
   BOOST_REQUIRE_EQUAL(change.main_changes.producer_count, 21);
   BOOST_REQUIRE_EQUAL(change.backup_changes.producer_count, 3);

   produce_block();
   hbs = control->head_block_state();
   header_exts =  hbs->header_exts;
   BOOST_REQUIRE_EQUAL( header_exts.count(producer_schedule_change_extension_v2::extension_id()) , 0 );
   BOOST_REQUIRE( hbs->pending_schedule.schedule.contains<uint32_t>());
   flat_map<name, block_signing_authority>   main_schedule;
   flat_map<name, block_signing_authority>   backup_schedule;
   get_producer_schedule(elected_producers, 21, 3, main_schedule, backup_schedule);

   auto main_active_schedule = producer_change_helper::producers_from(hbs->active_schedule);

   BOOST_REQUIRE_EQUAL( hbs->header.producer, N(amax) );
   BOOST_REQUIRE_EQUAL( hbs->active_schedule.version, 1 );
   // wdump((hbs->active_schedule.producers));
   // wdump((main_schedule));
   BOOST_REQUIRE( main_active_schedule == main_schedule);

   BOOST_REQUIRE( hbs->active_backup_schedule.schedule);
   BOOST_REQUIRE( !hbs->active_backup_schedule.pre_schedule || hbs->active_backup_schedule.pre_schedule->producers.empty() );
   BOOST_REQUIRE( hbs->active_backup_schedule.schedule == hbs->active_backup_schedule.get_schedule() );
   auto active_backup_schedule = *hbs->active_backup_schedule.schedule;

   BOOST_REQUIRE_EQUAL( active_backup_schedule.version, 1 );
   BOOST_REQUIRE( active_backup_schedule.producers == backup_schedule);

   producing_backup = true;

   produce_blocks(1);
   hbs = control->head_block_state();
   BOOST_REQUIRE( !hbs->active_backup_schedule.schedule && hbs->active_backup_schedule.pre_schedule );
   BOOST_REQUIRE_EQUAL( hbs->header.producer, calc_main_scheduled_producer(hbs->active_schedule.producers, hbs->header.timestamp).producer_name );

   auto old_hbs = hbs;
   auto old_backup_head_block = control->fork_db().get_backup_head_block(hbs->header.previous);
   BOOST_REQUIRE( old_backup_head_block );

   auto next_main_prod = calc_main_scheduled_producer(hbs->active_schedule.producers, hbs->header.timestamp.next()).producer_name;
   auto next_main_prod_info = get_producer_info(next_main_prod);
   wdump((next_main_prod_info));
   BOOST_REQUIRE_EQUAL(next_main_prod, next_main_prod_info.owner);
   BOOST_REQUIRE(next_main_prod_info.last_claimed_time < hbs->header.timestamp.to_time_point());
   BOOST_REQUIRE_EQUAL(next_main_prod_info.unclaimed_rewards, core_sym::from_string("0.0000"));
   BOOST_REQUIRE_GT(next_main_prod_info.ext.value.elected_votes, 0);
   BOOST_REQUIRE(next_main_prod_info.ext.value.reward_shared_ratio == 8000);

   auto backup_prod = calc_backup_scheduled_producer(
         hbs->active_backup_schedule.get_schedule(), hbs->header.timestamp)->producer_name;
   auto backup_prod_info = get_producer_info(backup_prod);

   BOOST_REQUIRE_EQUAL(backup_prod, old_backup_head_block->header.producer);
   BOOST_REQUIRE_EQUAL(backup_prod, backup_prod_info.owner);
   BOOST_REQUIRE(backup_prod_info.last_claimed_time < hbs->header.timestamp.to_time_point());
   BOOST_REQUIRE_EQUAL(backup_prod_info.unclaimed_rewards, core_sym::from_string("0.0000"));
   BOOST_REQUIRE_GT(backup_prod_info.ext.value.elected_votes, 0);
   BOOST_REQUIRE(backup_prod_info.ext.value.reward_shared_ratio == 8000);

   asset initial_inflation_per_block = CORE_ASSET(2'000'0000);

   setinflation(control->head_block_time(), initial_inflation_per_block);
   produce_block();

   hbs = control->head_block_state();

   BOOST_REQUIRE_EQUAL( hbs->header.backup_ext().is_backup, false );
   BOOST_REQUIRE( hbs->header.previous_backup() );
   BOOST_REQUIRE( !hbs->header.previous_backup()->id.empty() && bool(hbs->header.previous_backup()->producer) );
   BOOST_REQUIRE_EQUAL( hbs->header.previous_backup()->contribution, config::percent_100 );

   BOOST_REQUIRE_EQUAL(hbs->header.producer, next_main_prod);
   auto main_prod_info = get_producer_info(hbs->header.producer);
   wdump((main_prod_info));
   asset rewards_per_prod = CORE_ASSET(initial_inflation_per_block.get_amount() / 2);
   BOOST_REQUIRE_EQUAL(main_prod_info.unclaimed_rewards, rewards_per_prod);

   auto previous_backup_block = control->fork_db().get_block(hbs->header.previous_backup()->id);
   BOOST_REQUIRE( previous_backup_block );

   BOOST_REQUIRE_EQUAL( previous_backup_block, old_backup_head_block );
   BOOST_REQUIRE_EQUAL( previous_backup_block->header.producer, hbs->header.previous_backup()->producer );

   BOOST_REQUIRE_EQUAL( hbs->header.previous_backup()->producer, backup_prod );
   backup_prod_info = get_producer_info(backup_prod);

   BOOST_REQUIRE_EQUAL(backup_prod_info.unclaimed_rewards, rewards_per_prod);

   auto main_prod = next_main_prod;
   auto main_prod_balance = get_balance(main_prod);
   producer_claimrewards(main_prod);
   asset shared_rewards = CORE_ASSET(rewards_per_prod.get_amount() * 8000 / 10000);
   asset self_rewards = rewards_per_prod - shared_rewards;
   BOOST_REQUIRE_EQUAL( get_balance(main_prod), main_prod_balance + self_rewards );
   auto main_shared_reward_info = get_producer_shared_reward(main_prod);
   // wdump((main_shared_reward_info));
   BOOST_REQUIRE_EQUAL( main_shared_reward_info.unallocated_rewards, CORE_ASSET(0) );
   BOOST_REQUIRE_EQUAL( main_shared_reward_info.allocating_rewards, shared_rewards );
   BOOST_REQUIRE_EQUAL( main_shared_reward_info.votes, main_prod_info.ext.value.elected_votes );
   BOOST_REQUIRE( main_shared_reward_info.votes > 0 );
   BOOST_REQUIRE( main_shared_reward_info.rewards_per_vote == calc_rewards_per_vote(0, shared_rewards.get_amount(), main_shared_reward_info.votes));

   auto main_prod_voters = get_voters_of_producer(main_prod);
   auto main_voter_info = main_prod_voters.begin();
   auto main_voter = main_voter_info->first;

   auto main_voter_balance = get_balance(main_voter);
   auto main_voter_reward_info = get_voter_reward_info(main_voter);
   BOOST_REQUIRE_EQUAL( main_voter_reward_info.owner, main_voter );
   BOOST_REQUIRE_EQUAL( main_voter_reward_info.votes, main_voter_info->second );

   BOOST_REQUIRE( main_voter_reward_info.producers.find(main_prod) != main_voter_reward_info.producers.end() );
   BOOST_REQUIRE_EQUAL( main_voter_reward_info.unclaimed_rewards, CORE_ASSET(0) );
   BOOST_REQUIRE_EQUAL( main_voter_reward_info.claimed_rewards, CORE_ASSET(0) );

   voter_claimrewards(main_voter);

   main_voter_reward_info = get_voter_reward_info(main_voter);
   asset main_voter_rewards = CORE_ASSET( calc_voter_rewards(main_voter_reward_info.votes, main_shared_reward_info.rewards_per_vote) );

   BOOST_REQUIRE_EQUAL( main_voter_reward_info.claimed_rewards, main_voter_rewards );
   BOOST_REQUIRE_LT( main_voter_reward_info.claimed_rewards, shared_rewards );

   BOOST_REQUIRE_EQUAL( get_balance(main_voter), main_voter_balance + main_voter_rewards );

   regproducer( elected_producers[24].name );
   produce_block();

   // wdump( (elect_gstate) );
   BOOST_REQUIRE_EQUAL(elect_gstate.backup_elected_queue.last_producer_count, 3);
   BOOST_REQUIRE(elect_gstate.backup_elected_queue.tail_prev   == elected_producers[22]);
   BOOST_REQUIRE(elect_gstate.backup_elected_queue.tail        == elected_producers[23]);
   BOOST_REQUIRE(elect_gstate.backup_elected_queue.tail_next   == elected_producers[24]);

   regproducer( elected_producers[25].name );
   produce_block();
   elect_gstate = get_elect_global_state();

   // wdump( (elect_gstate) );
   BOOST_REQUIRE_EQUAL(elect_gstate.backup_elected_queue.last_producer_count, 4);
   BOOST_REQUIRE(elect_gstate.main_elected_queue.tail_next     == elected_producers[21]);
   BOOST_REQUIRE(elect_gstate.backup_elected_queue.tail_prev   == elected_producers[23]);
   // wdump( (elect_gstate.backup_elected_queue.tail)(elected_producers[24]) );
   BOOST_REQUIRE(elect_gstate.backup_elected_queue.tail        == elected_producers[24]);
   BOOST_REQUIRE(elect_gstate.backup_elected_queue.tail_next   == elected_producers[25]);

   for (size_t i = 0; i < voters.size(); i++) {
      if (i % 20 == 0) {
         produce_block();
      }
      auto voter = voters[voters.size() - i - 1];
      auto& voter_info = voter_map[voter];
      std::map<name, int64_t> producer_deltas;

      auto sub_votes = voter_info.last_votes;
      double old_votes_0 = 0.0;
      for (const auto& prod : voter_info.producers) {
         producer_deltas[prod] -= voter_info.last_votes;
      }

      voter_info.last_votes = voter_info.staked.get_amount();

      size_t max_count = std::min(30ul, producers.size());
      voter_info.producers.resize(max_count);
      for (size_t j = 0; j < max_count; j++) {
         const auto& prod = producers[ (i + j) % producers.size() ];
         voter_info.producers[j] = prod;
         producer_deltas[prod] += voter_info.last_votes;
      }
      std::sort( voter_info.producers.begin(), voter_info.producers.end());

      for (const auto& pd : producer_deltas) {
         auto& pv = producer_map[pd.first];
         pv.elected_votes += pd.second;
         if (pv.elected_votes < 0) pv.elected_votes = 0;
      }


      if( !vote( voter, voter_info.producers ) ) {
         BOOST_FAIL("vote failed");
      }
      // wdump( (elected_change_count_in_db()) );
   }
   produce_blocks();

   reopen();

   BOOST_REQUIRE_EQUAL(producing_backup, true);

   {
      hbs = control->head_block_state();
      elected_producers = get_elected_producers(producer_map);
      elected_producers_in_db = get_elected_producers_from_db(elect_gstate.elected_version);
      elect_gstate = get_elect_global_state();
      auto& meq = elect_gstate.main_elected_queue;
      auto& beq = elect_gstate.backup_elected_queue;

      // for (size_t i = 0; i < elected_producers_in_db.size(); i++)
      // {
      //    wdump( (i) (elected_producers_in_db[i]));
      // }

      // for (size_t i = 0; i < elected_producers.size(); i++)
      // wdump( (elected_producers.size() ));
      // for (size_t i = 0; i < 21; i++)
      // {
      //    wdump( (i) (elected_producers[i]));
      // }

      // wdump((elect_gstate));

      BOOST_REQUIRE_EQUAL(elect_gstate.main_elected_queue.last_producer_count, 21);
      BOOST_REQUIRE_GT(elect_gstate.backup_elected_queue.last_producer_count, 3);
      auto bpc = elect_gstate.main_elected_queue.last_producer_count + elect_gstate.backup_elected_queue.last_producer_count;

      BOOST_REQUIRE(elect_gstate.main_elected_queue.tail_prev     == elected_producers_in_db[19]);
      BOOST_REQUIRE(elect_gstate.main_elected_queue.tail          == elected_producers_in_db[20]);
      BOOST_REQUIRE(elect_gstate.main_elected_queue.tail_next     == elected_producers_in_db[21]);

      BOOST_REQUIRE(elect_gstate.backup_elected_queue.tail_prev   == elected_producers_in_db[bpc-2]);
      BOOST_REQUIRE(elect_gstate.backup_elected_queue.tail        == elected_producers_in_db[bpc-1]);
      BOOST_REQUIRE(elect_gstate.backup_elected_queue.tail_next   == elected_producers_in_db[bpc]);

      // wdump(  (elected_producers) );
      // wdump(  (elected_producers_in_db) );

      BOOST_REQUIRE( vector_matched(elected_producers, elected_producers_in_db, bpc) );

      // return;
      auto elected_change = get_elected_change_from_db();
      // wdump( (elected_change.size()) );
      // for (size_t i = 0; i < elected_change.size(); i++) {
      //    wdump((i)(elected_change[i]));
      // }

      BOOST_REQUIRE_GT(elected_change.size(), 0);

      main_schedule = producer_change_helper::producers_from(hbs->active_schedule);
      backup_schedule = hbs->active_backup_schedule.get_schedule()->producers;
      flat_map<name, block_signing_authority>   main_schedule_merged = main_schedule;
      flat_map<name, block_signing_authority>   backup_schedule_merged = backup_schedule;

      // wdump((main_schedule));
      // wdump((backup_schedule));


      get_producer_schedule(elected_producers, elect_gstate.main_elected_queue.last_producer_count,
         elect_gstate.backup_elected_queue.last_producer_count, main_schedule, backup_schedule);

      producer_change_helper::merge(elected_change, main_schedule_merged, backup_schedule_merged);

      BOOST_REQUIRE(main_schedule_merged == main_schedule);
      BOOST_REQUIRE(backup_schedule_merged == backup_schedule);
   }

   wdump((control->head_block_num()));
   BOOST_REQUIRE( produce_blocks_until(10000, [&]() {
         hbs = control->head_block_state();
         return elected_change_empty_in_db() &&
               !gpo.proposed_schedule_block_num &&
               hbs->pending_schedule.schedule.contains<uint32_t>();
   }) );
   wdump((control->head_block_num()));

   produce_block();

   reopen();
   produce_block();

}
FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
