#ifdef AMAX_CUSTODY_TESTS

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

class amax_custody_tester;
template<typename Tester>
class amax_token {
   // typedef amax_custody_tester Tester;
   using action_result = typename Tester::action_result;
public:
   amax_token(Tester &tester): t(tester) {
      t.produce_blocks( 2 );

      t.create_accounts( { N(amax.token) } );
      t.produce_blocks( 2 );

      t.set_code( N(amax.token), contracts::token_wasm() );
      t.set_abi( N(amax.token), contracts::token_abi().data() );

      t.produce_blocks();
      const auto& accnt =t.control->db().template get<account_object, by_name>( N(amax.token) );
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      abi_ser.set_abi(abi, abi_serializer::create_yield_function(t.abi_serializer_max_time));
   }

   action_result push_action( const account_name& signer, const action_name &name, const variant_object &data ) {
      return t.push_action(N(amax.token), abi_ser, signer, name, data);
   }

   fc::variant get_stats( const string& symbolname )
   {
      auto symb = eosio::chain::symbol::from_string(symbolname);
      auto symbol_code = symb.to_symbol_code().value;
      vector<char> data = t.get_row_by_account( N(amax.token), name(symbol_code), N(stat), account_name(symbol_code) );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "currency_stats", data, abi_serializer::create_yield_function(t.abi_serializer_max_time) );
   }

   fc::variant get_account( account_name acc, const string& symbolname)
   {
      auto symb = eosio::chain::symbol::from_string(symbolname);
      auto symbol_code = symb.to_symbol_code().value;
      vector<char> data = t.get_row_by_account( N(amax.token), acc, N(accounts), account_name(symbol_code) );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "account", data, abi_serializer::create_yield_function(t.abi_serializer_max_time) );
   }

   action_result create( account_name issuer,
                         asset        maximum_supply ) {

      return push_action( N(amax.token), N(create), mvo()
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
           ( "symbol", "0,CERO" )
      );
   }

   abi_serializer abi_ser;
   Tester &t;
};

class amax_custody_tester : public tester {
   using Token = amax_token<amax_custody_tester>;
public:

   amax_custody_tester() {
      token = std::make_unique<Token>(*this);
      produce_blocks( 2 );

      create_accounts( { N(plan.owner), N(issuer), N(receiver), N(amax.custody), N(fee.receiver) } );
      produce_blocks( 2 );

      set_code( N(amax.custody), contracts::custody_wasm() );
      set_abi( N(amax.custody), contracts::custody_abi().data() );

      produce_blocks();

      const auto& accnt = control->db().get<account_object,by_name>( N(amax.custody) );
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      abi_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));
      produce_blocks();

      BOOST_REQUIRE_EQUAL( success(),
         token->create( N(amax), asset::from_string("1000000000.00000000 AMAX"))
      );

      BOOST_REQUIRE_EQUAL( success(),
         token->issue( N(amax), asset::from_string("1000000000.00000000 AMAX"), "hola" )
      );

      produce_blocks(1);
   }

   action_result push_action( const name& contract, abi_serializer &abi_ser, const account_name& signer, const action_name &name, const variant_object &data ) {
      string action_type_name = abi_ser.get_action_type(name);

      action act;
      act.account = contract;
      act.name    = name;
      act.data    = abi_ser.variant_to_binary( action_type_name, data, abi_serializer::create_yield_function(abi_serializer_max_time) );

      return base_tester::push_action( std::move(act), signer.to_uint64_t() );
   }

   action_result push_action( const account_name& signer, const action_name &name, const variant_object &data ) {
      return push_action(N(amax.custody), abi_ser, signer, name, data);
   }

   action_result setconfig(const asset &plan_fee, const name &fee_receiver) {
      return push_action( N(amax.custody), N(setconfig), mvo()
           ( "plan_fee", plan_fee)
           ( "fee_receiver", fee_receiver)
      );
   }

   action_result addplan(const name& owner, const string& title, const name& asset_contract, const symbol& asset_symbol,
                        const uint64_t& unlock_interval_days, const int64_t& unlock_times)
   {
      return push_action( owner, N(addplan), mvo()
           ( "owner", owner)
           ( "title", title)
           ( "asset_contract", asset_contract)
           ( "asset_symbol", asset_symbol)
           ( "unlock_interval_days", unlock_interval_days)
           ( "unlock_times", unlock_times)
      );
   }


   action_result addissue(const name& issuer, const name& receiver, uint64_t plan_id,
                         uint64_t first_unlock_days, const asset& quantity)
   {
      return push_action( issuer, N(addissue), mvo()
           ( "issuer", issuer)
           ( "receiver", receiver)
           ( "plan_id", plan_id)
           ( "first_unlock_days", first_unlock_days)
           ( "quantity", quantity)
      );
   }


   action_result endissue(const name& issuer, const uint64_t& plan_id, const uint64_t& issue_id)
   {
      return push_action( issuer, N(endissue), mvo()
           ( "issuer", issuer)
           ( "plan_id", plan_id)
           ( "issue_id", issue_id)
      );
   }

   abi_serializer abi_ser;
   std::unique_ptr<Token>  token;
};

BOOST_AUTO_TEST_SUITE(amax_custody_tests)

BOOST_FIXTURE_TEST_CASE( custody_test, amax_custody_tester ) try {

   // auto token = create( N(amax), asset::from_string("1000000000.00000000 TKN"));
   // auto stats = get_stats("8,TKN");
   // REQUIRE_MATCHING_OBJECT( stats, mvo()
   //    ("supply", "0.000 TKN")
   //    ("max_supply", "1000.000 TKN")
   //    ("issuer", "alice")
   // );
   // produce_blocks(1);

} FC_LOG_AND_RETHROW()

BOOST_FIXTURE_TEST_CASE( end_issue, amax_custody_tester ) try {

   // auto token = create( N(amax), asset::from_string("1000000000.00000000 TKN"));
   // auto stats = get_stats("8,TKN");
   // REQUIRE_MATCHING_OBJECT( stats, mvo()
   //    ("supply", "0.000 TKN")
   //    ("max_supply", "1000.000 TKN")
   //    ("issuer", "alice")
   // );
   // produce_blocks(1);


   BOOST_REQUIRE_EQUAL( success(),
      setconfig(asset::from_string("2.00000000 AMAX"), N(fee.receiver))
   );


   BOOST_REQUIRE_EQUAL( success(),
      token->transfer( N(amax), N(plan.owner), asset::from_string("1000.00000000 AMAX"), "" )
   );

   BOOST_REQUIRE_EQUAL( success(),
      addplan(N(plan.owner), "my plan is best", N(amax.token), symbol(8, "AMAX"), 3, 10)
   );

   BOOST_REQUIRE_EQUAL( success(),
      token->transfer( N(plan.owner), N(amax.custody), asset::from_string("2.00000000 AMAX"), "plan:" )
   );

   BOOST_REQUIRE_EQUAL( success(),
      token->transfer( N(amax), N(issuer), asset::from_string("1000.00000000 AMAX"), "" )
   );

   BOOST_REQUIRE_EQUAL( success(),
      addissue(N(issuer), N(receiver), 1, 100, asset::from_string("100.00000000 AMAX"))
   );

   BOOST_REQUIRE_EQUAL( success(),
      token->transfer( N(issuer), N(amax.custody), asset::from_string("100.00000000 AMAX"), "issue:" )
   );

   BOOST_REQUIRE_EQUAL( success(),
      endissue(N(issuer), 1, 1)
   );

   wdump( (token->get_account(N(amax.custody), "8,AMAX"))
          (token->get_account(N(issuer), "8,AMAX"))
          (token->get_account(N(receiver), "8,AMAX"))
   );

} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()

#endif //AMAX_CUSTODY_TESTS