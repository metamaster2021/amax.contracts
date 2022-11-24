#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
// #include "amax.system_tester.hpp"
#include "contracts.hpp"

#include "Runtime/Runtime.h"

#include <fc/variant_object.hpp>
#include <eosio/chain/authorization_manager.hpp>

using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;
using namespace std;

using mvo = fc::mutable_variant_object;
using recover_target_type = static_variant<public_key_type, string>;
static auto global_contract_name  =  N(amax.proxy);

class amax_proxy_base_tester : public tester {
public:
   amax_proxy_base_tester() {
      produce_blocks( 2 );

      create_accounts( { N(alice), N(bob), N(carol) }, false, false );
      create_accounts( { global_contract_name } , false, true);
      produce_blocks( 2 );

      set_code( global_contract_name, contracts::proxy_wasm() );

      set_abi( global_contract_name, contracts::proxy_abi().data() );

      produce_blocks();

      const auto& accnt = control->db().get<account_object,by_name>( global_contract_name );
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      abi_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));

      produce_blocks();


   }

   action_result push_action( const account_name& signer, const action_name &name, const variant_object &data ) {
      string action_type_name = abi_ser.get_action_type(name);

      action act;
      act.account = global_contract_name;
      act.name    = name;
      act.data    = abi_ser.variant_to_binary( action_type_name, data, abi_serializer::create_yield_function(abi_serializer_max_time) );

      return base_tester::push_action( std::move(act), signer.to_uint64_t() );
   }
\

   fc::variant get_table_auditscore( const name& account )
   {
      return get_table_common("auditscore_t", N(auditscores), account);
   }

   fc::variant get_table_accountaudit( const name& account )
   {
      return get_table_common("accountaudit_t", N(accaudits), account );
   }

   fc::variant get_table_recoverorder( const uint64_t& order_id )
   {
      return get_table_common("recoverorder_t", N(orders), name(order_id) );
   }

   fc::variant get_table_global( )
   {
      return get_table_common("global_t", N(global), N(global));
   }

   fc::variant get_table_common(const string& table_def, const name& table_name, const name& pk )
   {
      vector<char> data = get_row_by_account( global_contract_name, global_contract_name, table_name, pk);
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( table_def, data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   }

   // fc::variant get_account( account_name acc, const string& symbolname)
   // {
   //    auto symb = eosio::chain::symbol::from_string(symbolname);
   //    auto symbol_code = symb.to_symbol_code().value;
   //    vector<char> data = get_row_by_account( global_contract_name, acc, N(accounts), account_name(symbol_code) );
   //    return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "account", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   // }

   action_result action_init( uint8_t score_limit ) {
      return push_action( global_contract_name, N(init), mvo()
           ( "score_limit", score_limit)
      );
   }

   action_result action_newaccount( const name& admin, const name& creator, const name& account, const authority& active ) {
      return push_action(  global_contract_name, N(setscore), mvo()
           ( "admin",      admin)
           ( "creator",    creator)
           ( "account",    account)
           ( "active",     active)
      );
   }
   action_result action_setauditor( const name& account, const set<name>& actions  ) {
      return push_action(  global_contract_name, N(setauditor), mvo()
           ( "account", account)
           ( "actions", actions)
      );
   }


   void sys_updateauth(const name& account,
                            const name& permission,
                            const name& parent,
                            const authority& auth) {
      tester::push_action(config::system_account_name, updateauth::get_name(),
      // TESTER::push_action(config::system_account_name, N(updateauthex),
            { permission_level{account, permission}}, mvo()
               ("account",    account)
               ("permission", permission)
               ("parent",     parent)
               ("auth",       auth) );
   };


   authority get_auth( const name& account, const name& permission) {
      const auto& permission_by_owner = control->db().get_index<eosio::chain::permission_index>().indices().get<eosio::chain::by_owner>();
      auto perm_itr = permission_by_owner.find(std::make_tuple(account, permission));
      BOOST_REQUIRE(perm_itr != permission_by_owner.end());
      return perm_itr->auth;
   };

   abi_serializer abi_ser;
};