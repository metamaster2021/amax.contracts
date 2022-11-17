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

class amax_recover_base_tester : public tester {
public:

   amax_recover_base_tester() {
      produce_blocks( 2 );

      create_accounts( { N(alice), N(bob), N(carol), N(amax.recover) } );
      produce_blocks( 2 );

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

   fc::variant get_table_accaudits( const name& account )
   {
      return get_table_common("accountaudit_t", N(accaudits), account);
   }

   fc::variant get_table_global( )
   {
      vector<char> data = get_row_by_account( N(amax.recover), N(amax.recover), N(global), N(global));
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "global_t", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   }

   fc::variant get_table_common(const string& table_def, const name& table_name, const name& pk )
   {
      vector<char> data = get_row_by_account( N(amax.recover), N(amax.recover), table_name, pk);
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( table_def, data, abi_serializer::create_yield_function(abi_serializer_max_time) );
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
   action_result action_setauditor( const name& account, const set<name>& actions  ) {
      return push_action(  N(amax.recover), N(setauditor), mvo()
           ( "account", account)
           ( "actions", actions)
      );
   }

   action_result action_bindaccount(  const name& admin, const name& account, const string& number_hash ) {
      return push_action(  N(amax.recover), N(bindaccount), mvo()
           ( "admin", admin)
           ( "account", account)
           ( "number_hash", number_hash)
      );
   }
   

   abi_serializer abi_ser;
};