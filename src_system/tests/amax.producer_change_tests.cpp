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


struct producer_elected_votes {
   name                    name;
   double                  elected_votes = 0.0;
   block_signing_authority authority;
   inline friend bool operator==(const producer_elected_votes& a, const producer_elected_votes& b)  { return std::tie(a.elected_votes, a.name, a.authority) == std::tie(b.elected_votes, b.name, b.authority); }
   inline friend bool operator!=(const producer_elected_votes& a, const producer_elected_votes& b)  { return !(a == b); }

};
FC_REFLECT( producer_elected_votes, (name)(elected_votes)(authority) )

struct producer_elected_queue {
   uint32_t                  last_producer_count = 0;
   producer_elected_votes    tail;
   producer_elected_votes    tail_prev;
   producer_elected_votes    tail_next;
};
FC_REFLECT( producer_elected_queue, (last_producer_count)(tail)(tail_prev)(tail_next) )

struct amax_global_state_ext {
   uint32_t                   max_main_producer_count        = 21;
   uint32_t                   max_backup_producer_count  = 10000;
   uint64_t                   last_producer_change_id    = 0;
   producer_elected_queue     main_elected_queue;
   producer_elected_queue     backup_elected_queue;
   uint8_t                    revision = 0; ///< used to track version updates in the future.
};
FC_REFLECT( amax_global_state_ext, (max_main_producer_count)(max_backup_producer_count)(last_producer_change_id)
                                   (main_elected_queue)(backup_elected_queue)(revision) )

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
};

FC_REFLECT( producer_info, (owner)(total_votes)(producer_key)(is_active)(url)(location)
                                    (last_claimed_time)(unclaimed_rewards)(producer_authority) )

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

   vector<producer_elected_votes> get_elected_producers(fc::flat_map<name, producer_elected_votes> &producer_map) {
      vector<producer_elected_votes> ret;
      ret.reserve(producer_map.size());
      for (const auto& p : producer_map) {
         ret.push_back(p.second);
      }
      std::sort( ret.begin(), ret.end(), []( const producer_elected_votes& lhs, const producer_elected_votes& rhs ) {
         auto lv = -lhs.elected_votes; auto rv = -rhs.elected_votes;
         return std::tie(lv, lhs.name) < std::tie(rv, rhs.name);
      } );
      return ret;
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

   vector<producer_elected_votes> get_elected_producers_from_db() {
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
         ret.push_back({info.owner, info.total_votes, info.producer_authority});
      }
      return ret;
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

   static const producer_elected_votes empty_elected_votes = {};

   auto producers = gen_producer_names(200, N(prod.1111111).to_uint64_t());
   auto voters = gen_producer_names(200, N(voter.111111).to_uint64_t());

   fc::flat_map<name, producer_elected_votes> producer_map;
   // wdump( (producers) );
   produce_block();
   auto ram_asset = core_sym::from_string("10000.0000");
   for (size_t i = 0; i < producers.size(); i++) {
      create_account_with_resources( producers[i], config::system_account_name, 10 * 1024 );
      regproducer( producers[i] );
      produce_block();
      producer_map[ producers[i] ] = { producers[i], 0, make_producer_authority(producers[i], 1) };
      // wdump( (producers[i]) (get_producer_info(producers[i])) );
   }

   auto votes_started = core_sym::from_string("1000000.0000");
   for (size_t i = 0; i < voters.size(); i++) {
      asset votes = CORE_ASSET(votes_started.get_amount() * (i + 1) );
      asset net = asset(votes.get_amount() / 2, CORE_SYMBOL);;
      asset cpu = votes - net;
      // wdump( (voters[i]) (ram_asset) (net) (cpu ));
      create_account_with_resources( voters[i], config::system_account_name, 10 * 1024);
      // wdump( (voters[i]) (votes) );
      transfer(N(amax), voters[i], votes);
      if(!stake( voters[i], net, cpu ) ) {
         BOOST_ERROR("stake failed");
      }
      size_t max_count = std::min(30ul, producers.size());
      vector<name> voted_producers(max_count);
      for (size_t j = 0; j < max_count; j++) {
         voted_producers[j] = producers[ (i + j) % producers.size() ];
         producer_map[ voted_producers[j] ].elected_votes += stake2votes(votes);
      }
      std::sort( voted_producers.begin(), voted_producers.end());

      if( !vote( voters[i], voted_producers ) ) {
         BOOST_ERROR("vote failed");
      }
      produce_block();
   }

   const auto& gpo = control->get_global_properties();

   initelects(config::system_account_name, 100);
   BOOST_REQUIRE(gpo.proposed_schedule_block_num);
   BOOST_REQUIRE_EQUAL(*gpo.proposed_schedule_block_num, control->head_block_num() + 1);
   BOOST_REQUIRE_EQUAL(gpo.proposed_schedule.version, 0);
   BOOST_REQUIRE_EQUAL(gpo.proposed_schedule.producers.size(), 0);
   BOOST_REQUIRE_EQUAL(gpo.proposed_schedule_change.version, 1);
   BOOST_REQUIRE_EQUAL(gpo.proposed_schedule_change.main_changes.producer_count, 21);
   BOOST_REQUIRE_EQUAL(gpo.proposed_schedule_change.backup_changes.producer_count, 0);

   produce_block();
   // wdump( (get_global_state()) );
   BOOST_REQUIRE( get_global_state()["ext"].is_object() );
   auto ext = get_ext(get_global_state()["ext"]);

   auto elected_producers = get_elected_producers(producer_map);
   auto elected_producers_in_db = get_elected_producers_from_db();
   // wdump(  (elected_producers) );
   // wdump(  (elected_producers_in_db) );
   BOOST_REQUIRE( elected_producers == elected_producers_in_db);
   // wdump( (ext) );
   BOOST_REQUIRE_EQUAL(ext.max_main_producer_count, 21);
   BOOST_REQUIRE_EQUAL(ext.max_backup_producer_count, 100);
   BOOST_REQUIRE_EQUAL(ext.main_elected_queue.last_producer_count, 21);
   BOOST_REQUIRE_EQUAL(ext.backup_elected_queue.last_producer_count, 0);

   // wdump( (get_rex_balance(ext.main_elected_queue.tail.name))(ext.main_elected_queue.tail) (elected_producers[20]) );
   BOOST_REQUIRE(ext.main_elected_queue.tail_prev     == elected_producers[19]);
   BOOST_REQUIRE(ext.main_elected_queue.tail          == elected_producers[20]);
   BOOST_REQUIRE(ext.main_elected_queue.tail_next     == elected_producers[21]);
   BOOST_REQUIRE(ext.backup_elected_queue.tail        == empty_elected_votes);
   BOOST_REQUIRE(ext.backup_elected_queue.tail_prev   == empty_elected_votes);
   BOOST_REQUIRE(ext.backup_elected_queue.tail_next   == empty_elected_votes);


} // weight_tests
FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
