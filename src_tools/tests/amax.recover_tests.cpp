#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
// #include "amax.system_tester.hpp"
#include "contracts.hpp"

#include "Runtime/Runtime.h"

#include <fc/variant_object.hpp>

using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;
using namespace std;

using mvo = fc::mutable_variant_object;

class amax_recover_tester : public tester {
public:

   amax_recover_tester() {
      produce_blocks( 2 );

      create_accounts( { N(alice), N(bob), N(carol), N(amax.recover) } );
      produce_blocks( 2 );

      wdump((contracts::recover_wasm()));

      
      set_code( N(amax.recover), contracts::recover_wasm() );

      set_abi( N(amax.recover), contracts::recover_abi().data() );

      produce_blocks();

      const auto& accnt = control->db().get<account_object,by_name>( N(amax.recover) );
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      abi_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));
   }

   action_result push_action( const account_name& signer, const action_name &name, const variant_object &data ) {
      string action_type_name = abi_ser.get_action_type(name);

      action act;
      act.account = N(amax.recover);
      act.name    = name;
      act.data    = abi_ser.variant_to_binary( action_type_name, data, abi_serializer::create_yield_function(abi_serializer_max_time) );

      return base_tester::push_action( std::move(act), signer.to_uint64_t() );
   }

   fc::variant get_table_auditor( const name& auditor )
   {
      vector<char> data = get_row_by_account( N(amax.recover), N(amax.recover), N(auditors), auditor );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "auditor_t", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   }


   fc::variant get_table_global( )
   {
      vector<char> data = get_row_by_account( N(amax.recover), N(amax.recover), N(global), N(global));
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "global_t", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   }

   // fc::variant get_account( account_name acc, const string& symbolname)
   // {
   //    auto symb = eosio::chain::symbol::from_string(symbolname);
   //    auto symbol_code = symb.to_symbol_code().value;
   //    vector<char> data = get_row_by_account( N(amax.recover), acc, N(accounts), account_name(symbol_code) );
   //    return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "account", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   // }

   action_result action_init( uint8_t score_limit ) {
      return push_action( N(amax.recover), N(init), mvo()
           ( "score_limit", score_limit)
      );
   }

   action_result action_setscore( name audit_type, int8_t score ) {
      return push_action(  N(amax.recover), N(setscore), mvo()
           ( "audit_type", audit_type)
           ( "score", score)
      );
   }

   abi_serializer abi_ser;
};

BOOST_AUTO_TEST_SUITE(amax_recover_tests)

BOOST_FIXTURE_TEST_CASE( init_tests, amax_recover_tester ) try {

   action_init( 5 );
   auto g = get_table_global();
   REQUIRE_MATCHING_OBJECT( g, mvo()
      ("score_limit", 5)
      ("last_order_id", 0)
   );
   produce_blocks(1);

} FC_LOG_AND_RETHROW()

// BOOST_FIXTURE_TEST_CASE( create_negative_max_supply, amax_recover_tester ) try {

//    BOOST_REQUIRE_EQUAL( wasm_assert_msg( "max-supply must be positive" ),
//       create( N(alice), asset::from_string("-1000.000 TKN"))
//    );

// } FC_LOG_AND_RETHROW()

// BOOST_FIXTURE_TEST_CASE( symbol_already_exists, amax_recover_tester ) try {

//    auto token = create( N(alice), asset::from_string("100 TKN"));
//    auto stats = get_stats("0,TKN");
//    REQUIRE_MATCHING_OBJECT( stats, mvo()
//       ("supply", "0 TKN")
//       ("max_supply", "100 TKN")
//       ("issuer", "alice")
//    );
//    produce_blocks(1);

//    BOOST_REQUIRE_EQUAL( wasm_assert_msg( "token with symbol already exists" ),
//                         create( N(alice), asset::from_string("100 TKN"))
//    );

// } FC_LOG_AND_RETHROW()

// BOOST_FIXTURE_TEST_CASE( create_max_supply, amax_recover_tester ) try {

//    auto token = create( N(alice), asset::from_string("4611686018427387903 TKN"));
//    auto stats = get_stats("0,TKN");
//    REQUIRE_MATCHING_OBJECT( stats, mvo()
//       ("supply", "0 TKN")
//       ("max_supply", "4611686018427387903 TKN")
//       ("issuer", "alice")
//    );
//    produce_blocks(1);

//    asset max(10, symbol(SY(0, NKT)));
//    share_type amount = 4611686018427387904;
//    static_assert(sizeof(share_type) <= sizeof(asset), "asset changed so test is no longer valid");
//    static_assert(std::is_trivially_copyable<asset>::value, "asset is not trivially copyable");
//    memcpy(&max, &amount, sizeof(share_type)); // hack in an invalid amount

//    BOOST_CHECK_EXCEPTION( create( N(alice), max) , asset_type_exception, [](const asset_type_exception& e) {
//       return expect_assert_message(e, "magnitude of asset amount must be less than 2^62");
//    });


// } FC_LOG_AND_RETHROW()

// BOOST_FIXTURE_TEST_CASE( create_max_decimals, amax_recover_tester ) try {

//    auto token = create( N(alice), asset::from_string("1.000000000000000000 TKN"));
//    auto stats = get_stats("18,TKN");
//    REQUIRE_MATCHING_OBJECT( stats, mvo()
//       ("supply", "0.000000000000000000 TKN")
//       ("max_supply", "1.000000000000000000 TKN")
//       ("issuer", "alice")
//    );
//    produce_blocks(1);

//    asset max(10, symbol(SY(0, NKT)));
//    //1.0000000000000000000 => 0x8ac7230489e80000L
//    share_type amount = 0x8ac7230489e80000L;
//    static_assert(sizeof(share_type) <= sizeof(asset), "asset changed so test is no longer valid");
//    static_assert(std::is_trivially_copyable<asset>::value, "asset is not trivially copyable");
//    memcpy(&max, &amount, sizeof(share_type)); // hack in an invalid amount

//    BOOST_CHECK_EXCEPTION( create( N(alice), max) , asset_type_exception, [](const asset_type_exception& e) {
//       return expect_assert_message(e, "magnitude of asset amount must be less than 2^62");
//    });

// } FC_LOG_AND_RETHROW()

// BOOST_FIXTURE_TEST_CASE( issue_tests, amax_recover_tester ) try {

//    auto token = create( N(alice), asset::from_string("1000.000 TKN"));
//    produce_blocks(1);

//    issue( N(alice), asset::from_string("500.000 TKN"), "hola" );

//    auto stats = get_stats("3,TKN");
//    REQUIRE_MATCHING_OBJECT( stats, mvo()
//       ("supply", "500.000 TKN")
//       ("max_supply", "1000.000 TKN")
//       ("issuer", "alice")
//    );

//    auto alice_balance = get_account(N(alice), "3,TKN");
//    REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
//       ("balance", "500.000 TKN")
//    );

//    BOOST_REQUIRE_EQUAL( wasm_assert_msg( "quantity exceeds available supply" ),
//                         issue( N(alice), asset::from_string("500.001 TKN"), "hola" )
//    );

//    BOOST_REQUIRE_EQUAL( wasm_assert_msg( "must issue positive quantity" ),
//                         issue( N(alice), asset::from_string("-1.000 TKN"), "hola" )
//    );

//    BOOST_REQUIRE_EQUAL( success(),
//                         issue( N(alice), asset::from_string("1.000 TKN"), "hola" )
//    );


// } FC_LOG_AND_RETHROW()

// BOOST_FIXTURE_TEST_CASE( retire_tests, amax_recover_tester ) try {

//    auto token = create( N(alice), asset::from_string("1000.000 TKN"));
//    produce_blocks(1);

//    BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("500.000 TKN"), "hola" ) );

//    auto stats = get_stats("3,TKN");
//    REQUIRE_MATCHING_OBJECT( stats, mvo()
//       ("supply", "500.000 TKN")
//       ("max_supply", "1000.000 TKN")
//       ("issuer", "alice")
//    );

//    auto alice_balance = get_account(N(alice), "3,TKN");
//    REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
//       ("balance", "500.000 TKN")
//    );

//    BOOST_REQUIRE_EQUAL( success(), retire( N(alice), asset::from_string("200.000 TKN"), "hola" ) );
//    stats = get_stats("3,TKN");
//    REQUIRE_MATCHING_OBJECT( stats, mvo()
//       ("supply", "300.000 TKN")
//       ("max_supply", "1000.000 TKN")
//       ("issuer", "alice")
//    );
//    alice_balance = get_account(N(alice), "3,TKN");
//    REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
//       ("balance", "300.000 TKN")
//    );

//    //should fail to retire more than current supply
//    BOOST_REQUIRE_EQUAL( wasm_assert_msg("overdrawn balance"), retire( N(alice), asset::from_string("500.000 TKN"), "hola" ) );

//    BOOST_REQUIRE_EQUAL( success(), transfer( N(alice), N(bob), asset::from_string("200.000 TKN"), "hola" ) );
//    //should fail to retire since tokens are not on the issuer's balance
//    BOOST_REQUIRE_EQUAL( wasm_assert_msg("overdrawn balance"), retire( N(alice), asset::from_string("300.000 TKN"), "hola" ) );
//    //transfer tokens back
//    BOOST_REQUIRE_EQUAL( success(), transfer( N(bob), N(alice), asset::from_string("200.000 TKN"), "hola" ) );

//    BOOST_REQUIRE_EQUAL( success(), retire( N(alice), asset::from_string("300.000 TKN"), "hola" ) );
//    stats = get_stats("3,TKN");
//    REQUIRE_MATCHING_OBJECT( stats, mvo()
//       ("supply", "0.000 TKN")
//       ("max_supply", "1000.000 TKN")
//       ("issuer", "alice")
//    );
//    alice_balance = get_account(N(alice), "3,TKN");
//    REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
//       ("balance", "0.000 TKN")
//    );

//    //trying to retire tokens with zero supply
//    BOOST_REQUIRE_EQUAL( wasm_assert_msg("overdrawn balance"), retire( N(alice), asset::from_string("1.000 TKN"), "hola" ) );

// } FC_LOG_AND_RETHROW()

// BOOST_FIXTURE_TEST_CASE( transfer_tests, amax_recover_tester ) try {

//    auto token = create( N(alice), asset::from_string("1000 CERO"));
//    produce_blocks(1);

//    issue( N(alice), asset::from_string("1000 CERO"), "hola" );

//    auto stats = get_stats("0,CERO");
//    REQUIRE_MATCHING_OBJECT( stats, mvo()
//       ("supply", "1000 CERO")
//       ("max_supply", "1000 CERO")
//       ("issuer", "alice")
//    );

//    auto alice_balance = get_account(N(alice), "0,CERO");
//    REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
//       ("balance", "1000 CERO")
//    );

//    transfer( N(alice), N(bob), asset::from_string("300 CERO"), "hola" );

//    alice_balance = get_account(N(alice), "0,CERO");
//    REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
//       ("balance", "700 CERO")
//       ("frozen", 0)
//       ("whitelist", 1)
//    );

//    auto bob_balance = get_account(N(bob), "0,CERO");
//    REQUIRE_MATCHING_OBJECT( bob_balance, mvo()
//       ("balance", "300 CERO")
//       ("frozen", 0)
//       ("whitelist", 1)
//    );

//    BOOST_REQUIRE_EQUAL( wasm_assert_msg( "overdrawn balance" ),
//       transfer( N(alice), N(bob), asset::from_string("701 CERO"), "hola" )
//    );

//    BOOST_REQUIRE_EQUAL( wasm_assert_msg( "must transfer positive quantity" ),
//       transfer( N(alice), N(bob), asset::from_string("-1000 CERO"), "hola" )
//    );


// } FC_LOG_AND_RETHROW()

// BOOST_FIXTURE_TEST_CASE( open_tests, amax_recover_tester ) try {

//    auto token = create( N(alice), asset::from_string("1000 CERO"));

//    auto alice_balance = get_account(N(alice), "0,CERO");
//    BOOST_REQUIRE_EQUAL(true, alice_balance.is_null() );
//    BOOST_REQUIRE_EQUAL( wasm_assert_msg("tokens can only be issued to issuer account"),
//                         push_action( N(alice), N(issue), mvo()
//                                      ( "to",       "bob")
//                                      ( "quantity", asset::from_string("1000 CERO") )
//                                      ( "memo",     "") ) );
//    BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("1000 CERO"), "issue" ) );

//    alice_balance = get_account(N(alice), "0,CERO");
//    REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
//       ("balance", "1000 CERO")
//    );

//    auto bob_balance = get_account(N(bob), "0,CERO");
//    BOOST_REQUIRE_EQUAL(true, bob_balance.is_null() );

//    BOOST_REQUIRE_EQUAL( wasm_assert_msg("owner account does not exist"),
//                         open( N(nonexistent), "0,CERO", N(alice) ) );
//    BOOST_REQUIRE_EQUAL( success(),
//                         open( N(bob),         "0,CERO", N(alice) ) );

//    bob_balance = get_account(N(bob), "0,CERO");
//    REQUIRE_MATCHING_OBJECT( bob_balance, mvo()
//       ("balance", "0 CERO")
//    );

//    BOOST_REQUIRE_EQUAL( success(), transfer( N(alice), N(bob), asset::from_string("200 CERO"), "hola" ) );

//    bob_balance = get_account(N(bob), "0,CERO");
//    REQUIRE_MATCHING_OBJECT( bob_balance, mvo()
//       ("balance", "200 CERO")
//    );

//    BOOST_REQUIRE_EQUAL( wasm_assert_msg( "symbol does not exist" ),
//                         open( N(carol), "0,INVALID", N(alice) ) );

//    BOOST_REQUIRE_EQUAL( wasm_assert_msg( "symbol precision mismatch" ),
//                         open( N(carol), "1,CERO", N(alice) ) );

// } FC_LOG_AND_RETHROW()

// BOOST_FIXTURE_TEST_CASE( close_tests, amax_recover_tester ) try {

//    auto token = create( N(alice), asset::from_string("1000 CERO"));

//    auto alice_balance = get_account(N(alice), "0,CERO");
//    BOOST_REQUIRE_EQUAL(true, alice_balance.is_null() );

//    BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("1000 CERO"), "hola" ) );

//    alice_balance = get_account(N(alice), "0,CERO");
//    REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
//       ("balance", "1000 CERO")
//    );

//    BOOST_REQUIRE_EQUAL( success(), transfer( N(alice), N(bob), asset::from_string("1000 CERO"), "hola" ) );

//    alice_balance = get_account(N(alice), "0,CERO");
//    REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
//       ("balance", "0 CERO")
//    );

//    BOOST_REQUIRE_EQUAL( success(), close( N(alice), "0,CERO" ) );
//    alice_balance = get_account(N(alice), "0,CERO");
//    BOOST_REQUIRE_EQUAL(true, alice_balance.is_null() );

// } FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
