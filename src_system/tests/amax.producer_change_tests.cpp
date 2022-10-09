#include <Runtime/Runtime.h>
#include <boost/test/unit_test.hpp>
#include <cstdlib>
#include <eosio/chain/contract_table_objects.hpp>
#include <eosio/chain/exceptions.hpp>
#include <eosio/chain/global_property_object.hpp>
#include <eosio/chain/resource_limits.hpp>
#include <eosio/chain/wast_to_wasm.hpp>
#include <eosio/chain/config.hpp>
#include <fc/log/logger.hpp>
#include <iostream>
#include <sstream>

#include "amax.system_tester.hpp"


static const fc::microseconds block_interval_us = fc::microseconds(eosio::chain::config::block_interval_us);

namespace producer_change_helper {
   uint8_t elected_version = 0;
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

static float128_t by_votes_prod(const name& owner, double elected_votes, uint8_t elected_version, bool is_active = true) {
   if (elected_version == 0) {
      return f128_positive_infinity();
   }

   if (elected_votes < 0.0) elected_votes = 0.0;
   static constexpr uint64_t uint64_max = std::numeric_limits<uint64_t>::max();
   float128_t reversed = to_softfloat128(elected_votes) + to_softfloat128(uint64_max - owner.to_uint64_t()) / to_softfloat128(uint64_max);
   // wdump( (to_softfloat128(elected_votes)) );
   // wdump( (owner)(owner.to_uint64_t()) (uint64_max - owner.to_uint64_t()) (to_softfloat128(uint64_max - owner.to_uint64_t()) / to_softfloat128(uint64_max)) );
   // wdump( (to_softfloat128(elected_votes) + to_softfloat128(uint64_max - owner.to_uint64_t()) / to_softfloat128(uint64_max)) );
   // wdump( (reversed) );

   return is_active ? to_softfloat128(0) - reversed : f128_positive_infinity() - reversed;
}

struct producer_elected_votes {
   name                    name;
   double                  elected_votes = 0.0;
   block_signing_authority authority;
   bool empty() const {
      return !bool(name);
   }

};
FC_REFLECT( producer_elected_votes, (name)(elected_votes)(authority) )


inline float128_t by_votes_prod(const producer_elected_votes& v) {
   return by_votes_prod(v.name, v.elected_votes, producer_change_helper::elected_version);
}

inline bool operator<(const producer_elected_votes& a, const producer_elected_votes& b)  {
   return by_votes_prod(a) > by_votes_prod(b);
}

inline bool operator>(const producer_elected_votes& a, const producer_elected_votes& b)  {
   return by_votes_prod(a) < by_votes_prod(b);
}

inline bool operator<=(const producer_elected_votes& a, const producer_elected_votes& b)  {
      return !(a > b);
}
inline bool operator>=(const producer_elected_votes& a, const producer_elected_votes& b)  {
   return !(a < b);
}
inline bool operator==(const producer_elected_votes& a, const producer_elected_votes& b)  {
   return a.name == b.name && a.elected_votes == b.elected_votes;
}
inline bool operator!=(const producer_elected_votes& a, const producer_elected_votes& b)  {
   return !(a == b);
}


struct producer_elected_queue {
   uint32_t                  last_producer_count = 0;
   producer_elected_votes    tail;
   producer_elected_votes    tail_prev;
   producer_elected_votes    tail_next;
};
FC_REFLECT( producer_elected_queue, (last_producer_count)(tail)(tail_prev)(tail_next) )

struct amax_global_state_ext {
   uint8_t                    elected_version            = 0;
   uint32_t                   max_main_producer_count    = 21;
   uint32_t                   max_backup_producer_count  = 10000;
   uint64_t                   last_producer_change_id    = 0;
   producer_elected_queue     main_elected_queue;
   producer_elected_queue     backup_elected_queue;
};
FC_REFLECT( amax_global_state_ext, (elected_version)(max_main_producer_count)(max_backup_producer_count)(last_producer_change_id)
                                   (main_elected_queue)(backup_elected_queue) )

struct producer_info_ext {
   // uint8_t        elected_version   = 0;
   double         elected_votes     = 0;
};
FC_REFLECT( producer_info_ext, /*(elected_version)*/(elected_votes))

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

vector<account_name> gen_producer_names(uint32_t count, uint64_t from) {
   vector<account_name> result;
   from >>= 4;
   for (uint64_t n = from; result.size() < count; n++) {
      result.emplace_back(n << 4);
   }
   return result;
}

using namespace eosio_system;

struct producer_change_tester : eosio_system_tester {

   struct voted_info_t {
      vector<name> producers;
      asset staked;
      asset net;
      asset cpu;
   };

   vector<account_name> producers = gen_producer_names(100, N(prod.1111111).to_uint64_t());
   vector<account_name> voters = gen_producer_names(100, N(voter.111111).to_uint64_t());
   fc::flat_map<name, producer_elected_votes> producer_map;
   fc::flat_map<name, voted_info_t> voter_map;


   // push action without commit current block
   transaction_trace_ptr push_action( const account_name& signer, const action_name &name, const variant_object &data ) {
      return base_tester::push_action( config::system_account_name, name, signer, data);
   }

   // void initelects( const name& payer, uint32_t max_backup_producer_count );
   transaction_trace_ptr initelects(const name& payer, uint32_t max_backup_producer_count) {
      // push action without commit current block
      return push_action(payer, N(initelects), mvo()
                  ("payer", payer)
                  ("max_backup_producer_count", max_backup_producer_count)
      );
   }

   amax_global_state_ext get_ext(const fc::variant &v) const {
      amax_global_state_ext ret;
      // fc::from_variant(v, ret);
      auto data = abi_ser.variant_to_binary("amax_global_state_ext", v, abi_serializer::create_yield_function( abi_serializer_max_time ));
      return fc::raw::unpack<amax_global_state_ext>(data);
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
      block_signing_private_keys[pubkey] = privkey;
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

   vector<producer_elected_votes> get_elected_producers(fc::flat_map<name, producer_elected_votes> &producer_map,
                     size_t max_size = -1) {
      vector<producer_elected_votes> ret;
      ret.reserve(producer_map.size());
      for (const auto& p : producer_map) {
         ret.push_back(p.second);
      }
      std::sort( ret.begin(), ret.end(), []( const producer_elected_votes& a, const producer_elected_votes& b ) {
         return a > b;
      } );

      auto sz = std::min(max_size, producer_map.size());
      return vector<producer_elected_votes>(ret.begin(), ret.begin() + sz);
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

   vector<producer_elected_votes> get_elected_producers_from_db(uint8_t min_elected_version, size_t max_size = -1) {
      vector<producer_elected_votes> ret;
      const auto& db = control->db();
      const auto& idx_id = get_table_index_id(config::system_account_name, config::system_account_name, N(producers), 1);
      const auto& t_id = get_table_id(config::system_account_name, config::system_account_name, N(producers) );
      const auto& idx = db.get_index<eosio::chain::index_long_double_index, eosio::chain::by_secondary>();

      float128_t lowest = eosio::chain::secondary_key_traits<float128_t>::true_lowest();
      vector<char> data;
      auto itr = idx.lower_bound( boost::make_tuple( idx_id.id, lowest, std::numeric_limits<uint64_t>::lowest() ) );
      for (; itr != idx.end() && itr->t_id == idx_id.id; itr++) {
         const auto* itr2 = db.find<eosio::chain::key_value_object, eosio::chain::by_scope_primary>( boost::make_tuple(t_id.id, itr->primary_key) );
         EOS_ASSERT(itr2 != nullptr, eosio::chain::contract_table_query_exception,
                  "primary data not found by key:" + std::to_string(itr->primary_key));
         data.resize( itr2->value.size() );
         memcpy( data.data(), itr2->value.data(), itr2->value.size() );
         producer_info info = fc::raw::unpack<producer_info>(data);
         if (info.ext.value.elected_votes <= 0) {
            wdump( (info.owner) (info.ext.value.elected_votes));
            break;
         }
         ret.push_back({info.owner, info.total_votes, info.producer_authority});
      }
      auto sz = std::min(max_size, ret.size());
      return vector<producer_elected_votes>(ret.begin(), ret.begin() + sz);
   }

   void get_producer_schedule(const vector<producer_elected_votes>& elected_producers, uint32_t main_producer_count,
            uint32_t backup_producer_count, vector<producer_authority> &main_schedule,
            flat_map<name, block_signing_authority> &backup_schedule) {
      main_schedule.clear();
      backup_schedule.clear();
      for (size_t i = 0; i < main_producer_count; i++) {
         main_schedule.push_back({elected_producers[i].name, elected_producers[i].authority});
      }
      std::sort(main_schedule.begin(), main_schedule.end(), []( const auto& lhs, const auto& rhs ) {
         return lhs.producer_name < rhs.producer_name; // sort by producer name
      } );
      for (size_t i = main_producer_count; i < main_producer_count + backup_producer_count; i++) {
         backup_schedule.emplace(elected_producers[i].name, elected_producers[i].authority);
      }
   }

   producer_authority calc_main_scheduled_producer( const vector<producer_authority> &producers, block_timestamp_type t ) {
      auto index = t.slot % (producers.size() * config::producer_repetitions);
      index /= config::producer_repetitions;
      return producers[index];
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

};

template <typename A, typename B, typename D>
bool near(A a, B b, D delta) {
   if (abs(a - b) <= delta)
      return true;
   elog("near: ${a} ${b}", ("a", a)("b", b));
   return false;
}

BOOST_AUTO_TEST_SUITE(producer_change_tests)

BOOST_FIXTURE_TEST_CASE(init_elects_test, producer_change_tester) try {
   produce_block();

   producer_change_helper::elected_version = 1;
   // producer_elected_votes a = { N(prod.1111144), 2.799331929972997e+19, {} };
   // producer_elected_votes b = { N(prod.1111143), 2.766591205645827e+19, {} };

   // producer_elected_votes a = { N(prod.111111x), 3.196586051809321e+19, {} };
   // producer_elected_votes b = { N(prod.111112n), 3.196586051809321e+19, {} };

   // wdump( (a) );
   // wdump( (b) );
   // auto av = by_votes_prod(a);
   // auto bv = by_votes_prod(b);
   // wdump ( ( av ) );
   // wdump ( ( bv ) );
   // // wdump ( ( a > b ));
   // // wdump ( ( a < b ));
   // wdump ( ( av > bv ));
   // wdump ( ( av < bv ));

   // return;

   // wdump( (producers) );
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

   auto total_staked = core_sym::min_activated_stake;
   for (size_t i = 0; i < voters.size(); i++) {
      auto const& voter = voters[i];
      auto& voter_info = voter_map[voter];
      double base_amount = total_staked.get_amount() / (2.0 * voters.size());
      voter_info.staked = CORE_ASSET(base_amount * (1.0 + (i + 1.0) / voters.size()) );
      voter_info.net = CORE_ASSET(voter_info.staked.get_amount() / 2);
      voter_info.cpu = voter_info.staked - voter_info.net;
      // wdump( (voters[i]) (ram_asset) (net) (cpu ));
      create_account_with_resources( voter, config::system_account_name, 10 * 1024);
      // wdump( (voters[i]) (votes) );
      transfer(N(amax), voters[i], voter_info.staked);
      if(!stake( voter, voter_info.net, voter_info.cpu ) ) {
         BOOST_ERROR("stake failed");
      }
      size_t max_count = std::min(30ul, producers.size());
      voter_info.producers.resize(max_count);
      for (size_t j = 0; j < max_count; j++) {
         const auto& prod = producers[ (i + j) % producers.size() ];
         voter_info.producers[j] = prod;
         producer_map[prod].elected_votes += stake2votes(voter_info.staked);
      }
      std::sort( voter_info.producers.begin(), voter_info.producers.end());

      if( !vote( voter, voter_info.producers ) ) {
         BOOST_ERROR("vote failed");
      }
      if (i % 20 == 0)
         produce_block();
   }
   produce_block();

   const auto& gpo = control->get_global_properties();

   initelects(config::system_account_name, 43);
   BOOST_REQUIRE(gpo.proposed_schedule_block_num);
   BOOST_REQUIRE_EQUAL(*gpo.proposed_schedule_block_num, control->head_block_num() + 1);
   BOOST_REQUIRE_EQUAL(gpo.proposed_schedule.version, 0);
   BOOST_REQUIRE_EQUAL(gpo.proposed_schedule.producers.size(), 0);
   BOOST_REQUIRE_EQUAL(gpo.proposed_schedule_change.version, 1);
   BOOST_REQUIRE_EQUAL(gpo.proposed_schedule_change.main_changes.producer_count, 21);
   BOOST_REQUIRE_EQUAL(gpo.proposed_schedule_change.backup_changes.producer_count, 3);

   produce_block();
   // wdump( (get_global_state()) );
   BOOST_REQUIRE( get_global_state()["ext"].is_object() );
   auto ext = get_ext(get_global_state()["ext"]);

   // wdump( (ext) );
   BOOST_REQUIRE_EQUAL(ext.elected_version, 1);
   BOOST_REQUIRE_EQUAL(ext.max_main_producer_count, 21);
   BOOST_REQUIRE_EQUAL(ext.max_backup_producer_count, 43);
   BOOST_REQUIRE_EQUAL(ext.main_elected_queue.last_producer_count, 21);
   BOOST_REQUIRE_EQUAL(ext.backup_elected_queue.last_producer_count, 3);

   producer_change_helper::elected_version = 1;
   auto elected_producers = get_elected_producers(producer_map);
   auto elected_producers_in_db = get_elected_producers_from_db(ext.elected_version);
   // wdump(  (elected_producers) );
   // wdump(  (elected_producers_in_db) );
   // BOOST_REQUIRE_EQUAL(elected_producers_in_db.size(), 21);

   BOOST_REQUIRE( vector_matched(elected_producers, elected_producers_in_db, 21) );

   // wdump( (get_rex_balance(ext.main_elected_queue.tail.name))(ext.main_elected_queue.tail) (elected_producers[20]) );
   // wdump( (ext.main_elected_queue.tail_prev)(elected_producers[19]) );
   BOOST_REQUIRE(ext.main_elected_queue.tail_prev     == elected_producers[19]);
   BOOST_REQUIRE(ext.main_elected_queue.tail          == elected_producers[20]);
   BOOST_REQUIRE(ext.main_elected_queue.tail_next     == elected_producers[21]);
   BOOST_REQUIRE(ext.backup_elected_queue.tail_prev   == elected_producers[22]);
   BOOST_REQUIRE(ext.backup_elected_queue.tail        == elected_producers[23]);
   BOOST_REQUIRE(ext.backup_elected_queue.tail_next   == elected_producers[24]);

   produce_block();
   auto hbs = control->head_block_state();
   auto header_exts = hbs->header_exts;
   wdump( (hbs));

   BOOST_REQUIRE(!gpo.proposed_schedule_block_num);
   BOOST_REQUIRE_EQUAL( header_exts.count(producer_schedule_change_extension_v2::extension_id()) , 1 );
   const auto& new_producer_schedule = header_exts.lower_bound(producer_schedule_change_extension_v2::extension_id())->second.get<producer_schedule_change_extension_v2>();

   wdump((new_producer_schedule));
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
   vector<producer_authority>                main_schedule;
   flat_map<name, block_signing_authority>   backup_schedule;
   get_producer_schedule(elected_producers, 21, 3, main_schedule, backup_schedule);

   auto active_schedule = hbs->active_schedule;

   BOOST_REQUIRE_EQUAL( hbs->header.producer, N(amax) );
   BOOST_REQUIRE_EQUAL( active_schedule.version, 1 );
   wdump((active_schedule.producers));
   wdump((main_schedule));
   BOOST_REQUIRE( active_schedule.producers == main_schedule);

   BOOST_REQUIRE( hbs->active_backup_schedule.schedule && !hbs->active_backup_schedule.pre_schedule );
   BOOST_REQUIRE( hbs->active_backup_schedule.schedule == hbs->active_backup_schedule.get_schedule() );
   auto active_backup_schedule = *hbs->active_backup_schedule.schedule;

   BOOST_REQUIRE_EQUAL( active_backup_schedule.version, 1 );
   BOOST_REQUIRE( active_backup_schedule.producers == backup_schedule);

   produce_blocks(1);
   hbs = control->head_block_state();
   BOOST_REQUIRE( !hbs->active_backup_schedule.schedule && hbs->active_backup_schedule.pre_schedule );
   BOOST_REQUIRE_EQUAL( hbs->header.producer, calc_main_scheduled_producer(main_schedule, hbs->header.timestamp).producer_name );

   regproducer( elected_producers[24].name );
   produce_block();
   ext = get_ext(get_global_state()["ext"]);

   wdump( (ext) );
   BOOST_REQUIRE_EQUAL(ext.backup_elected_queue.last_producer_count, 3);
   BOOST_REQUIRE(ext.backup_elected_queue.tail_prev   == elected_producers[22]);
   BOOST_REQUIRE(ext.backup_elected_queue.tail        == elected_producers[23]);
   BOOST_REQUIRE(ext.backup_elected_queue.tail_next   == elected_producers[24]);

   regproducer( elected_producers[25].name );
   produce_block();
   ext = get_ext(get_global_state()["ext"]);

   wdump( (ext) );
   BOOST_REQUIRE_EQUAL(ext.backup_elected_queue.last_producer_count, 4);
   BOOST_REQUIRE(ext.main_elected_queue.tail_next     == elected_producers[21]);
   BOOST_REQUIRE(ext.backup_elected_queue.tail_prev   == elected_producers[23]);
   wdump( (ext.backup_elected_queue.tail)(elected_producers[24]) );
   BOOST_REQUIRE(ext.backup_elected_queue.tail        == elected_producers[24]);
   BOOST_REQUIRE(ext.backup_elected_queue.tail_next   == elected_producers[25]);

   producer_map = {};
   for (size_t i = 0; i < voters.size(); i++) {
      auto voter = voters[voters.size() - i - 1];
      auto& voter_info = voter_map[voter];
      size_t max_count = std::min(30ul, producers.size());
      voter_info.producers.resize(max_count);
      for (size_t j = 0; j < max_count; j++) {
         const auto& prod = producers[ (i + j) % producers.size() ];
         voter_info.producers[j] = prod;
         producer_map[prod].elected_votes += stake2votes(voter_info.staked);
      }
      std::sort( voter_info.producers.begin(), voter_info.producers.end());

      if( !vote( voter, voter_info.producers ) ) {
         BOOST_ERROR("vote failed");
      }

      // if (i % 20 == 0)
         produce_block();
   }
   produce_block();


} // weight_tests
FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
