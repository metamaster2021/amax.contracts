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

class amax_recover_base_tester : public tester {
public:

   amax_recover_base_tester() {
      produce_blocks( 2 );

      // create_accounts( { N(alice), N(bob), N(carol) }, false, false );
      create_accounts( { N(amax.recover) } , false, true);
      produce_blocks( 2 );

      set_code( N(amax.recover), contracts::recover_wasm() );

      set_abi( N(amax.recover), contracts::recover_abi().data() );

      produce_blocks();

      const auto& accnt = control->db().get<account_object,by_name>( N(amax.recover) );
      abi_def abi;
      // BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      abi_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));

      produce_blocks();


   }

   action_result push_action( const account_name& signer, const action_name &name, const variant_object &data ) {
      string action_type_name = abi_ser.get_action_type(name);

      action act;
      act.account = N(amax.recover);
      act.name    = name;
      act.data    = abi_ser.variant_to_binary( action_type_name, data, abi_serializer::create_yield_function(abi_serializer_max_time) );

      return base_tester::push_action( std::move(act), signer.to_uint64_t() );
   }

   fc::variant get_table_checker( const name& checker )
   {
      vector<char> data = get_row_by_account( N(amax.recover), N(amax.recover), N(checkers), checker );
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "checker_t", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   }

   fc::variant get_table_auditscore( const name& account )
   {
      return get_table_common("audit_conf_t", N(auditscores), account);
   }

   fc::variant get_table_accountaudit( const name& account )
   {
      return get_table_common("accountaudit_t", N(accaudits), account );
   }

   fc::variant get_table_recoverorder( const uint64_t& order_id )
   {
      return get_table_common("recover_order_t", N(orders), name(order_id) );
   }

   fc::variant get_table_global( )
   {
      return get_table_common("global_t", N(global), N(global));
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

   action_result action_init( uint8_t recover_threshold ) {
      return push_action( N(amax.recover), N(init), mvo()
           ( "recover_threshold", recover_threshold)
      );
   }

   action_result action_setscore( name audit_type, int8_t score ) {
      return push_action(  N(amax.recover), N(setscore), mvo()
           ( "audit_type", audit_type)
           ( "score", score)
      );
   }
   action_result action_setchecker( const name& account, const set<name>& actions  ) {
      return push_action(  N(amax.recover), N(setchecker), mvo()
           ( "account", account)
           ( "actions", actions)
      );
   }

   action_result action_bindaccount(  const name& admin, const name& account, const string& number_hash ) {
      return push_action(  N(amax.recover), N(bindaccount), mvo()
           ( "admin",         admin)
           ( "account",       account)
           ( "number_hash",   number_hash)
      );
   }


   action_result action_bindanswer( const name& admin, const name& account, map<uint8_t, string>& answers ) {
      return push_action(  N(amax.recover), N(bindanswer), mvo()
           ( "admin",      admin)
           ( "account",    account)
           ( "answers",    answers)
      );
   }

   action_result action_createorder(   const name&          admin,
                                       const name&          account,
                                       const string&        mobile_hash,
                                       const fc::variants&  recover_target,
                                       const bool&          manual_check_required) {
      return push_action(  N(amax.recover), N(createorder), mvo()
           ( "admin",            admin)
           ( "account",          account)
           ( "mobile_hash",      mobile_hash)
           ( "recover_target",   recover_target)
           ( "manual_check_required", manual_check_required)
      );
   }

   action_result action_chkanswer( const name&  admin,
                        const uint64_t&         order_id,
                        const name&             account,
                        const int8_t&           score) {
      return push_action(  N(amax.recover), N(chkanswer), mvo()
           ( "admin",      admin)
           ( "order_id",   order_id)
           ( "account",    account)
           ( "score",      score)
      );
   }
               

   action_result action_chkdid( const name& admin,
                        const uint64_t& order_id,
                        const name& account,
                        const bool& passed) {
      return push_action(  N(amax.recover), N(chkdid), mvo()
           ( "admin",      admin)
           ( "order_id",   order_id)
           ( "account",    account)
           ( "passed",     passed)
      );

   }

   action_result action_chkmanual( const name& admin,
                        const uint64_t& order_id,
                        const name& account,
                        const bool& passed) {

      return push_action(  N(amax.recover), N(chkmanual), mvo()
           ( "admin",      admin)
           ( "order_id",   order_id)
           ( "account",    account)
           ( "passed",     passed)
      );
   }

   action_result action_closeorder( const name& submitter, const uint64_t& order_id ) {
       return push_action( submitter, N(closeorder), mvo()
           ( "submitter", submitter)
           ( "order_id", order_id)
      );
   }

   action_result action_delorder( const name& submitter, const uint64_t& order_id ) {
      return push_action(  N(amax.recover), N(delorder), mvo()
           ( "submitter",  submitter)
           ( "order_id",   order_id)
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