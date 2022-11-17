#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
// #include "amax.system_tester.hpp"
#include "contracts.hpp"

#include "Runtime/Runtime.h"

#include <fc/variant_object.hpp>
#include "amax_recover_base_tester.hpp"

using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;
using namespace std;

using mvo = fc::mutable_variant_object;

class amax_recover_tester : public amax_recover_base_tester {
public:

   amax_recover_tester() {
      init_contract();
   }
   
   void init_contract() {
      action_init( 5 );
      auto g = get_table_global();
      REQUIRE_MATCHING_OBJECT( g, mvo()
         ("score_limit", 5)
         ("last_order_id", 0)
      );
      produce_blocks(1);

      action_setscore(N("mobileno"), 3);
      produce_blocks(1);

      action_setscore(N("answer"), 3);
      produce_blocks(1);

      action_setscore(N("did"), 3);
      produce_blocks(1);

      set<name> actions = {N(bindaccount), N(bindanswer), N(createorder), N(chkanswer), N(chkdid), N(chkmanual)};
      action_setauditor(N(admin), actions);
      produce_blocks(1);
   }
};

BOOST_AUTO_TEST_SUITE(amax_recover_tests)

BOOST_FIXTURE_TEST_CASE( init_tests, amax_recover_tester ) try {


   action_bindaccount(N(admin), N(account), "mobile_hash" );
   produce_blocks(1);
   std::cout << "action_bindaccount ---- end \n" ; 
   // auto accaudit = get_table_accaudits(N(account));
   // wdump(("accaudit:")(accaudit));

   map<uint8_t, string> answers = {{1, "answer1"}, {2, "answer2"}};
   action_bindanswer (N(admin), N(account), answers );
   std::cout << "action_bindanswer ---- end\n"; 


   // action_createorder( N(admin) , N(account),"mobile_hash", "AM8CYknq1nMZxsuz6ZE85ihp8g3ddp4QNfc6nCV9mfBuVmLLeUSp", false);
   // std::cout << "action_createorder ---- end\n"; 

   action_chkanswer(N(admin), 1, N(account),2);
   std::cout << "action_chkanswer ---- end\n"; 

   action_chkdid(N(admin), 1, N(account), true);
   std::cout << "action_chkdid ---- end\n"; 

   action_chkmanual(N(admin), 1, N(account), true);
   std::cout << "action_chkmanual ---- end\n"; 

   action_closeorder(N(test), 1);
   std::cout << "action_closeorder ---- end\n"; 


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
