#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
#include "amax.system_tester.hpp"

#include "Runtime/Runtime.h"

#include <fc/variant_object.hpp>

using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;
using namespace std;

using mvo = fc::mutable_variant_object;

#define SYMB(P,X) symbol(P,#X)

#define REQUIRE_MATCHING_OBJECT_INVERSE(left, right) REQUIRE_MATCHING_OBJECT(right, left)

class amax_xtoken_tester : public tester {
public:

   amax_xtoken_tester() {
      produce_blocks( 2 );

      create_accounts( { N(alice), N(bob), N(carol), N(fee.receiver), N(amax.xtoken), N(deposit) } );
      produce_blocks( 2 );

      set_code( N(amax.xtoken), contracts::xtoken_wasm() );
      set_abi( N(amax.xtoken), contracts::xtoken_abi().data() );

      produce_blocks();

      const auto& accnt = control->db().get<account_object,by_name>( N(amax.xtoken) );
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      abi_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));
   }

   action_result push_action( const account_name& signer, const action_name &name, const variant_object &data ) {
      string action_type_name = abi_ser.get_action_type(name);

      action act;
      act.account = N(amax.xtoken);
      act.name    = name;
      act.data    = abi_ser.variant_to_binary( action_type_name, data, abi_serializer::create_yield_function(abi_serializer_max_time) );

      return base_tester::push_action( std::move(act), signer.to_uint64_t() );
   }

   action_result push_deposit_action(const account_name& signer, const action_name &name, const variant_object &data ) {
      auto deposit_abi_ser = get_deposit_abi_serializer();
      string action_type_name = deposit_abi_ser.get_action_type(name);

      action act;
      act.account = N(deposit);
      act.name    = name;
      act.data    = deposit_abi_ser.variant_to_binary( action_type_name, data, abi_serializer::create_yield_function(abi_serializer_max_time) );

      return base_tester::push_action( std::move(act), signer.to_uint64_t() );
   }

   fc::variant get_stats( const string& symbolname )
   {
      auto symb = eosio::chain::symbol::from_string(symbolname);
      auto symbol_code = symb.to_symbol_code().value;
      vector<char> data = get_row_by_account( N(amax.xtoken), name(symbol_code), N(stat), account_name(symbol_code) );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "currency_stats", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   }

   fc::variant get_account( account_name acc, const string& symbolname)
   {
      auto symb = eosio::chain::symbol::from_string(symbolname);
      auto symbol_code = symb.to_symbol_code().value;
      vector<char> data = get_row_by_account( N(amax.xtoken), acc, N(accounts), account_name(symbol_code) );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "account", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   }

   abi_serializer get_deposit_abi_serializer() const {
      abi_serializer deposit_abi_ser;
      const auto& accnt = control->db().get<account_object,by_name>( N(deposit) );
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      deposit_abi_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));
      return deposit_abi_ser;
   }

   fc::variant get_deposit_account( account_name acc, const symbol& symb)
   {
      auto symbol_code = symb.to_symbol_code().value;
      vector<char> data = get_row_by_account( N(deposit), acc, N(accounts), account_name(symbol_code) );
      return data.empty() ? fc::variant() : get_deposit_abi_serializer().binary_to_variant(
            "account", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   }

   action_result create( account_name issuer,
                         asset        maximum_supply ) {

      return push_action( N(amax.xtoken), N(create), mvo()
           ( "issuer", issuer)
           ( "maximum_supply", maximum_supply)
      );
   }

   action_result issue( account_name issuer, asset quantity, string memo ) {
      return push_action( issuer, N(issue), mvo()
           ( "issuer", issuer)
           ( "quantity", quantity)
           ( "memo", memo)
      );
   }

   action_result retire( account_name issuer, asset quantity, string memo ) {
      return push_action( issuer, N(retire), mvo()
           ( "quantity", quantity)
           ( "memo", memo)
      );

   }

   action_result transfer( account_name from,
                  account_name to,
                  asset        quantity,
                  string       memo ) {
      return push_action( from, N(transfer), mvo()
           ( "from", from)
           ( "to", to)
           ( "quantity", quantity)
           ( "memo", memo)
      );
   }

   action_result notifypayfee( account_name from,
                  account_name to,
                  asset        fee,
                  string       memo ) {
      return push_action( from, N(notifypayfee), mvo()
           ( "from", from)
           ( "to", to)
           ( "fee", fee)
           ( "memo", memo)
      );
   }

   action_result open( account_name owner,
                       const string& symbolname,
                       account_name ram_payer    ) {
      return push_action( ram_payer, N(open), mvo()
           ( "owner", owner )
           ( "symbol", symbolname )
           ( "ram_payer", ram_payer )
      );
   }

   action_result close( account_name owner,
                        const string& symbolname ) {
      return push_action( owner, N(close), mvo()
           ( "owner", owner )
           ( "symbol", symbolname )
      );
   }

   action_result feeratio( account_name issuer, const symbol &symbol, uint64_t fee_ratio ) {
      return push_action( issuer, N(feeratio), mvo()
           ( "symbol", symbol )
           ( "fee_ratio", fee_ratio )
      );
   }

   action_result feereceiver( account_name issuer, const symbol &symbol, const name &fee_receiver ) {
      return push_action( issuer, N(feereceiver), mvo()
           ( "symbol", symbol )
           ( "fee_receiver", fee_receiver )
      );
   }

   action_result feeexempt( account_name issuer, const symbol &symbol, const name &account, bool is_fee_exempt ) {
      return push_action( issuer, N(feeexempt), mvo()
           ( "symbol", symbol )
           ( "account", account )
           ( "is_fee_exempt", is_fee_exempt )
      );
   }

   action_result pause( account_name issuer, const symbol &symbol, bool is_paused ) {
      return push_action( issuer, N(pause), mvo()
           ( "symbol", symbol )
           ( "is_paused", is_paused )
      );
   }

   action_result freezeacct( account_name issuer, const symbol &symbol, const name &account, bool is_frozen ) {
      return push_action( issuer, N(freezeacct), mvo()
           ( "symbol", symbol )
           ( "account", account )
           ( "is_frozen", is_frozen )
      );
   }

   abi_serializer abi_ser;
};

BOOST_AUTO_TEST_SUITE(amax_xtoken_tests)

BOOST_FIXTURE_TEST_CASE( create_tests, amax_xtoken_tester ) try {

   auto token = create( N(alice), asset::from_string("1000.000 TKN"));
   auto stats = get_stats("3,TKN");
   REQUIRE_MATCHING_OBJECT_INVERSE( stats, mvo()
      ("supply", "0.000 TKN")
      ("max_supply", "1000.000 TKN")
      ("issuer", "alice")
   );
   produce_blocks(1);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( create_negative_max_supply, amax_xtoken_tester ) try {

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "max-supply must be positive" ),
      create( N(alice), asset::from_string("-1000.000 TKN"))
   );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( symbol_already_exists, amax_xtoken_tester ) try {

   auto token = create( N(alice), asset::from_string("100 TKN"));
   auto stats = get_stats("0,TKN");
   REQUIRE_MATCHING_OBJECT_INVERSE( stats, mvo()
      ("supply", "0 TKN")
      ("max_supply", "100 TKN")
      ("issuer", "alice")
   );
   produce_blocks(1);

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "token with symbol already exists" ),
                        create( N(alice), asset::from_string("100 TKN"))
   );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( create_max_supply, amax_xtoken_tester ) try {

   auto token = create( N(alice), asset::from_string("4611686018427387903 TKN"));
   auto stats = get_stats("0,TKN");
   REQUIRE_MATCHING_OBJECT_INVERSE( stats, mvo()
      ("supply", "0 TKN")
      ("max_supply", "4611686018427387903 TKN")
      ("issuer", "alice")
   );
   produce_blocks(1);

   asset max(10, symbol(SY(0, NKT)));
   share_type amount = 4611686018427387904;
   static_assert(sizeof(share_type) <= sizeof(asset), "asset changed so test is no longer valid");
   static_assert(std::is_trivially_copyable<asset>::value, "asset is not trivially copyable");
   memcpy(&max, &amount, sizeof(share_type)); // hack in an invalid amount

   BOOST_CHECK_EXCEPTION( create( N(alice), max) , asset_type_exception, [](const asset_type_exception& e) {
      return expect_assert_message(e, "magnitude of asset amount must be less than 2^62");
   });


} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( create_max_decimals, amax_xtoken_tester ) try {

   auto token = create( N(alice), asset::from_string("1.000000000000000000 TKN"));
   auto stats = get_stats("18,TKN");
   REQUIRE_MATCHING_OBJECT_INVERSE( stats, mvo()
      ("supply", "0.000000000000000000 TKN")
      ("max_supply", "1.000000000000000000 TKN")
      ("issuer", "alice")
   );
   produce_blocks(1);

   asset max(10, symbol(SY(0, NKT)));
   //1.0000000000000000000 => 0x8ac7230489e80000L
   share_type amount = 0x8ac7230489e80000L;
   static_assert(sizeof(share_type) <= sizeof(asset), "asset changed so test is no longer valid");
   static_assert(std::is_trivially_copyable<asset>::value, "asset is not trivially copyable");
   memcpy(&max, &amount, sizeof(share_type)); // hack in an invalid amount

   BOOST_CHECK_EXCEPTION( create( N(alice), max) , asset_type_exception, [](const asset_type_exception& e) {
      return expect_assert_message(e, "magnitude of asset amount must be less than 2^62");
   });

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( issue_tests, amax_xtoken_tester ) try {

   auto token = create( N(alice), asset::from_string("1000.000 TKN"));
   produce_blocks(1);

   issue( N(alice), asset::from_string("500.000 TKN"), "hola" );

   auto stats = get_stats("3,TKN");
   REQUIRE_MATCHING_OBJECT_INVERSE( stats, mvo()
      ("supply", "500.000 TKN")
      ("max_supply", "1000.000 TKN")
      ("issuer", "alice")
   );

   auto alice_balance = get_account(N(alice), "3,TKN");
   REQUIRE_MATCHING_OBJECT_INVERSE( alice_balance, mvo()
      ("balance", "500.000 TKN")
   );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "quantity exceeds available supply" ),
                        issue( N(alice), asset::from_string("500.001 TKN"), "hola" )
   );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "must issue positive quantity" ),
                        issue( N(alice), asset::from_string("-1.000 TKN"), "hola" )
   );

   BOOST_REQUIRE_EQUAL( success(),
                        issue( N(alice), asset::from_string("1.000 TKN"), "hola" )
   );


} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( retire_tests, amax_xtoken_tester ) try {

   auto token = create( N(alice), asset::from_string("1000.000 TKN"));
   produce_blocks(1);

   BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("500.000 TKN"), "hola" ) );

   auto stats = get_stats("3,TKN");
   REQUIRE_MATCHING_OBJECT_INVERSE( stats, mvo()
      ("supply", "500.000 TKN")
      ("max_supply", "1000.000 TKN")
      ("issuer", "alice")
   );

   auto alice_balance = get_account(N(alice), "3,TKN");
   REQUIRE_MATCHING_OBJECT_INVERSE( alice_balance, mvo()
      ("balance", "500.000 TKN")
   );

   BOOST_REQUIRE_EQUAL( success(), retire( N(alice), asset::from_string("200.000 TKN"), "hola" ) );
   stats = get_stats("3,TKN");
   REQUIRE_MATCHING_OBJECT_INVERSE( stats, mvo()
      ("supply", "300.000 TKN")
      ("max_supply", "1000.000 TKN")
      ("issuer", "alice")
   );
   alice_balance = get_account(N(alice), "3,TKN");
   REQUIRE_MATCHING_OBJECT_INVERSE( alice_balance, mvo()
      ("balance", "300.000 TKN")
   );

   //should fail to retire more than current supply
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("overdrawn balance"), retire( N(alice), asset::from_string("500.000 TKN"), "hola" ) );

   BOOST_REQUIRE_EQUAL( success(), transfer( N(alice), N(bob), asset::from_string("200.000 TKN"), "hola" ) );
   //should fail to retire since tokens are not on the issuer's balance
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("overdrawn balance"), retire( N(alice), asset::from_string("300.000 TKN"), "hola" ) );
   //transfer tokens back
   BOOST_REQUIRE_EQUAL( success(), transfer( N(bob), N(alice), asset::from_string("200.000 TKN"), "hola" ) );

   BOOST_REQUIRE_EQUAL( success(), retire( N(alice), asset::from_string("300.000 TKN"), "hola" ) );
   stats = get_stats("3,TKN");
   REQUIRE_MATCHING_OBJECT_INVERSE( stats, mvo()
      ("supply", "0.000 TKN")
      ("max_supply", "1000.000 TKN")
      ("issuer", "alice")
   );
   alice_balance = get_account(N(alice), "3,TKN");
   REQUIRE_MATCHING_OBJECT_INVERSE( alice_balance, mvo()
      ("balance", "0.000 TKN")
   );

   //trying to retire tokens with zero supply
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("overdrawn balance"), retire( N(alice), asset::from_string("1.000 TKN"), "hola" ) );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( transfer_tests, amax_xtoken_tester ) try {

   auto token = create( N(alice), asset::from_string("1000 CERO"));
   produce_blocks(1);

   issue( N(alice), asset::from_string("1000 CERO"), "hola" );

   auto stats = get_stats("0,CERO");
   REQUIRE_MATCHING_OBJECT_INVERSE( stats, mvo()
      ("supply", "1000 CERO")
      ("max_supply", "1000 CERO")
      ("issuer", "alice")
   );

   auto alice_balance = get_account(N(alice), "0,CERO");
   REQUIRE_MATCHING_OBJECT_INVERSE( alice_balance, mvo()
      ("balance", "1000 CERO")
   );

   transfer( N(alice), N(bob), asset::from_string("300 CERO"), "hola" );

   alice_balance = get_account(N(alice), "0,CERO");
   REQUIRE_MATCHING_OBJECT_INVERSE( alice_balance, mvo()
      ("balance", "700 CERO")
   );

   auto bob_balance = get_account(N(bob), "0,CERO");
   REQUIRE_MATCHING_OBJECT_INVERSE( bob_balance, mvo()
      ("balance", "300 CERO")
   );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "overdrawn balance" ),
      transfer( N(alice), N(bob), asset::from_string("701 CERO"), "hola" )
   );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "must transfer positive quantity" ),
      transfer( N(alice), N(bob), asset::from_string("-1000 CERO"), "hola" )
   );


} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( open_tests, amax_xtoken_tester ) try {

   auto token = create( N(alice), asset::from_string("1000 CERO"));

   auto alice_balance = get_account(N(alice), "0,CERO");
   BOOST_REQUIRE_EQUAL(true, alice_balance.is_null() );
   BOOST_REQUIRE_EQUAL( wasm_assert_msg("tokens can only be issued to issuer account"),
                        push_action( N(alice), N(issue), mvo()
                                     ( "issuer",       "bob")
                                     ( "quantity", asset::from_string("1000 CERO") )
                                     ( "memo",     "") ) );
   BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("1000 CERO"), "issue" ) );

   alice_balance = get_account(N(alice), "0,CERO");
   REQUIRE_MATCHING_OBJECT_INVERSE( alice_balance, mvo()
      ("balance", "1000 CERO")
   );

   auto bob_balance = get_account(N(bob), "0,CERO");
   BOOST_REQUIRE_EQUAL(true, bob_balance.is_null() );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg("owner account does not exist"),
                        open( N(nonexistent), "0,CERO", N(alice) ) );
   BOOST_REQUIRE_EQUAL( success(),
                        open( N(bob),         "0,CERO", N(alice) ) );

   bob_balance = get_account(N(bob), "0,CERO");
   REQUIRE_MATCHING_OBJECT_INVERSE( bob_balance, mvo()
      ("balance", "0 CERO")
   );

   BOOST_REQUIRE_EQUAL( success(), transfer( N(alice), N(bob), asset::from_string("200 CERO"), "hola" ) );

   bob_balance = get_account(N(bob), "0,CERO");
   REQUIRE_MATCHING_OBJECT_INVERSE( bob_balance, mvo()
      ("balance", "200 CERO")
   );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "token of symbol does not exist" ),
                        open( N(carol), "0,INVALID", N(alice) ) );

   BOOST_REQUIRE_EQUAL( wasm_assert_msg( "symbol precision mismatch" ),
                        open( N(carol), "1,CERO", N(alice) ) );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( close_tests, amax_xtoken_tester ) try {

   auto token = create( N(alice), asset::from_string("1000 CERO"));

   auto alice_balance = get_account(N(alice), "0,CERO");
   BOOST_REQUIRE_EQUAL(true, alice_balance.is_null() );

   BOOST_REQUIRE_EQUAL( success(), issue( N(alice), asset::from_string("1000 CERO"), "hola" ) );

   alice_balance = get_account(N(alice), "0,CERO");
   REQUIRE_MATCHING_OBJECT_INVERSE( alice_balance, mvo()
      ("balance", "1000 CERO")
   );

   BOOST_REQUIRE_EQUAL( success(), transfer( N(alice), N(bob), asset::from_string("1000 CERO"), "hola" ) );

   alice_balance = get_account(N(alice), "0,CERO");
   REQUIRE_MATCHING_OBJECT_INVERSE( alice_balance, mvo()
      ("balance", "0 CERO")
   );

   BOOST_REQUIRE_EQUAL( success(), close( N(alice), "0,CERO" ) );
   alice_balance = get_account(N(alice), "0,CERO");
   BOOST_REQUIRE_EQUAL(true, alice_balance.is_null() );

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( transfer_fee_tests, amax_xtoken_tester ) try {

   auto token = create( N(alice), asset::from_string("1000.0000 CERO"));
   produce_blocks(1);

   // config token
   feeratio( N(alice), SYMB(4,CERO), 30); // 0.3%, boost 10000
   feereceiver( N(alice), SYMB(4,CERO), N(fee.receiver));

   issue( N(alice), asset::from_string("1000.0000 CERO"), "hola" );

   auto stats = get_stats("4,CERO");
   REQUIRE_MATCHING_OBJECT( stats, mvo()
      ("supply", "1000.0000 CERO")
      ("max_supply", "1000.0000 CERO")
      ("issuer", "alice")
      ("is_paused", 0)
      ("fee_receiver", "fee.receiver")
      ("fee_ratio", 30 )
      ("min_fee_quantity", "0.0000 CERO")
   );

   auto alice_balance = get_account(N(alice), "4,CERO");
   REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
      ("balance", "1000.0000 CERO")
      ("is_frozen", false)
      ("is_fee_exempt", false)
   );

   BOOST_REQUIRE_EQUAL( success(),
      open( N(bob), "4,CERO",  N(alice) )
   );
   BOOST_REQUIRE_EQUAL( success(),
      feeexempt(N(alice), SYMB(4,CERO), N(bob), true)
   );

   // bob should have token: 300 + 300 * 0.003 = 300.9
   BOOST_REQUIRE_EQUAL( success(),
      transfer( N(alice), N(bob), asset::from_string("300.9000 CERO"), "bob is in whitelist, no fee" )
   );


   alice_balance = get_account(N(alice), "4,CERO");
   REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
      ("balance", "699.1000 CERO")
      ("is_frozen", false)
      ("is_fee_exempt", false)
   );

   auto bob_balance = get_account(N(bob), "4,CERO");
   REQUIRE_MATCHING_OBJECT( bob_balance, mvo()
      ("balance", "300.9000 CERO")
      ("is_frozen", false)
      ("is_fee_exempt", true)
   );

   BOOST_REQUIRE_EQUAL( success(),
      transfer( N(bob), N(carol), asset::from_string("300.0000 CERO"), "carol must pay fee" )
   );

   bob_balance = get_account(N(bob), "4,CERO");
   REQUIRE_MATCHING_OBJECT( bob_balance, mvo()
      ("balance", "0.9000 CERO")
      ("is_frozen", false)
      ("is_fee_exempt", true)
   );

   auto carol_balance = get_account(N(carol), "4,CERO");
   REQUIRE_MATCHING_OBJECT( carol_balance, mvo()
      ("balance", "299.1000 CERO")
      ("is_frozen", false)
      ("is_fee_exempt", false)
   );

   auto fee_receiver_balance = get_account(N(fee.receiver), "4,CERO");
      REQUIRE_MATCHING_OBJECT( fee_receiver_balance, mvo()
         ("balance", "0.9000 CERO")
         ("is_frozen", false)
         ("is_fee_exempt", false)
      );

} FC_LOG_AND_RETHROW()


BOOST_FIXTURE_TEST_CASE( deposit, amax_xtoken_tester ) try {

   auto rlm = control->get_resource_limits_manager();

   set_code( N(deposit), contracts::util::xtoken_deposit_wasm() );
   set_abi( N(deposit), contracts::util::xtoken_deposit_abi().data() );

   produce_blocks();

   auto token = create( N(alice), asset::from_string("1000.0000 CNY"));
   produce_blocks(1);

   // config token
   feeratio( N(alice), SYMB(4,CNY), 30); // 0.3%, boost 10000
   feereceiver( N(alice), SYMB(4,CNY), N(fee.receiver));

   issue( N(alice), asset::from_string("1000.0000 CNY"), "hola" );

   auto alice_balance = get_account(N(alice), "4,CNY");
   REQUIRE_MATCHING_OBJECT( alice_balance, mvo()
      ("balance", "1000.0000 CNY")
      ("is_frozen", false)
      ("is_fee_exempt", false)
   );

   BOOST_REQUIRE_EQUAL( success(),
      transfer( N(alice), N(bob), asset::from_string("300.0000 CNY"), "bob is in whitelist, no fee" )
   );

   // payed fee=300.0000*0.003=0.9
   auto bob_balance = get_account(N(bob), "4,CNY");
   REQUIRE_MATCHING_OBJECT( bob_balance, mvo()
      ("balance", "299.1000 CNY")
      ("is_frozen", false)
      ("is_fee_exempt", false)
   );

   BOOST_REQUIRE_EQUAL( success(),
      transfer( N(bob), N(deposit), asset::from_string("100.0000 CNY"), "deposit" )
   );

   wdump( ( get_deposit_account(N(bob), SYMB(4,CNY)) ) );
   REQUIRE_MATCHING_OBJECT( get_deposit_account(N(bob), SYMB(4,CNY)), mvo()
      ("balance", "99.7000 CNY")
   );
   BOOST_REQUIRE_EQUAL( success(),
      open( N(carol), "4,CNY",  N(carol) )
   );

   auto bob_ram_usage = rlm.get_account_ram_usage(N(bob));
   auto deposit_ram_usage = rlm.get_account_ram_usage(N(deposit));
   wdump( (rlm.get_account_ram_usage(N(bob))) (rlm.get_account_ram_usage(N(deposit))) );

   BOOST_REQUIRE_EQUAL( success(),
      push_deposit_action( N(bob), N(withdraw), mvo()
         ( "owner", "bob")
         ( "to", "carol")
         ( "quantity", "10.0000 CNY")
         ( "memo", "withdraw")
      )
   );
   produce_blocks();

   wdump( (rlm.get_account_ram_usage(N(bob))) (rlm.get_account_ram_usage(N(deposit))) );
   BOOST_REQUIRE_EQUAL(rlm.get_account_ram_usage(N(bob)) - bob_ram_usage,
                       deposit_ram_usage - rlm.get_account_ram_usage(N(deposit)) );

   REQUIRE_MATCHING_OBJECT( get_deposit_account(N(bob), SYMB(4,CNY)), mvo()
      ("balance", "89.7000 CNY")
   );

   wdump( (get_account(N(carol), "4,CNY")) );
   REQUIRE_MATCHING_OBJECT( get_account(N(carol), "4,CNY"), mvo()
      ("balance", "9.9700 CNY")
      ("is_frozen", false)
      ("is_fee_exempt", false)
   );

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
