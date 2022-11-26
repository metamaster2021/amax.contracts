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
static auto proxy_contract_name  =  N(amax.proxy);
static auto recover_contract_name  =  N(amax.recover);
static auto check1_contract_name  =  N(amax.check1);
static auto check2_contract_name  =  N(amax.check2);
static auto check3_contract_name  =  N(amax.check3);
static auto check4_contract_name  =  N(amax.check4);

#define PUSH_ACTION( contract,  ...)  printf("hello" contract "\n", ##__VA_ARGS__);


   // return push_action(  global_contract_name, N(setscore), mvo()
   //         ( "admin",      admin)
   //         ( "creator",    creator)
   //         ( "account",    account)
   //         ( "active",     active)
   //    );
class amax_proxy_base_tester : public tester {
public:
   abi_serializer proxy_abi_ser;
   abi_serializer recover_abi_ser;
   abi_serializer checker_abi_ser;

   amax_proxy_base_tester() {

      initialize_proxy_contract( proxy_contract_name );

      initialize_recover_contract( recover_contract_name);


      initialize_checker_contract( check1_contract_name);
      initialize_checker_contract( check2_contract_name);
      initialize_checker_contract( check3_contract_name);
      initialize_checker_contract( check4_contract_name);
      produce_blocks( 2 );

      initialize_abi_ser(check1_contract_name, checker_abi_ser);
      produce_blocks( 2 );

      checker_action_init(check1_contract_name, recover_contract_name);
      produce_blocks( 2 );

      checker_action_init(check2_contract_name, recover_contract_name);
      checker_action_init(check3_contract_name, recover_contract_name);
      checker_action_init(check4_contract_name, recover_contract_name);

      produce_blocks( 2 );

   }

   void initialize_recover_contract( name contract_name ) {
      initialize_contract(contract_name, contracts::recover_wasm(), contracts::recover_abi());
       produce_blocks( 2 );

      initialize_abi_ser(contract_name, recover_abi_ser);
      produce_blocks( 2 );

      recover_action_init(5, proxy_contract_name);
      produce_blocks( 2 );

   }

   void initialize_proxy_contract( name contract_name ) {
      initialize_contract(contract_name, contracts::proxy_wasm(), contracts::proxy_abi());
      produce_blocks( 2 );

      initialize_abi_ser(contract_name, proxy_abi_ser);
       produce_blocks( 2 );

      proxy_action_init(contract_name);
       produce_blocks( 2 );

   }

   void initialize_checker_contract( name contract_name) {
      initialize_contract(contract_name, contracts::checker_wasm(), contracts::checker_abi());
   }

   void initialize_contract( name contract_name, std::vector<uint8_t> wasm_file, std::vector<char> abi_file ) {

       produce_blocks( 2 );

      create_accounts( { contract_name } , false, true);
      produce_blocks( 2 );

      set_code( contract_name, wasm_file );

      set_abi( contract_name, abi_file.data() );

      produce_blocks();
   }

   void initialize_abi_ser( name contract_name, abi_serializer& abi_ser ) {
      const auto& accnt = control->db().get<account_object,by_name>( contract_name );
      abi_def abi;
      BOOST_REQUIRE_EQUAL(abi_serializer::to_abi(accnt.abi, abi), true);
      abi_ser.set_abi(abi, abi_serializer::create_yield_function(abi_serializer_max_time));
      produce_blocks();
   }

   action_result push_action( name contract_name,  abi_serializer abi_ser,  account_name signer,  action_name name, const variant_object data ) {
      string action_type_name = abi_ser.get_action_type(name);

      action act;
      act.account = contract_name;
      act.name    = name;
      act.data    = abi_ser.variant_to_binary( action_type_name, data, abi_serializer::create_yield_function(abi_serializer_max_time) );

      return base_tester::push_action( std::move(act), signer.to_uint64_t() );
   }

   fc::variant get_table_common(const name contract_name,const abi_serializer& abi_ser, const string& table_def, const name& table_name, const name& pk )
   {
      vector<char> data = get_row_by_account( contract_name, contract_name, table_name, pk);
      return data.empty() ? fc::variant() : abi_ser.binary_to_variant( table_def, data, abi_serializer::create_yield_function(abi_serializer_max_time) );
   }



//    fc::variant get_table_accountaudit( const name& account )
//    {
//       return get_table_common("accountaudit_t", N(accaudits), account );
//    }

//    fc::variant get_table_recoverorder( const uint64_t& order_id )
//    {
//       return get_table_common("recoverorder_t", N(orders), name(order_id) );
//    }

//    fc::variant get_table_global( )
//    {
//       return get_table_common("global_t", N(global), N(global));
//    }



//    // fc::variant get_account( account_name acc, const string& symbolname)
//    // {
//    //    auto symb = eosio::chain::symbol::from_string(symbolname);
//    //    auto symbol_code = symb.to_symbol_code().value;
//    //    vector<char> data = get_row_by_account( global_contract_name, acc, N(accounts), account_name(symbol_code) );
//    //    return data.empty() ? fc::variant() : abi_ser.binary_to_variant( "account", data, abi_serializer::create_yield_function(abi_serializer_max_time) );
//    // }

   action_result recover_action_init( uint8_t score_limit, 
                                    name amax_proxy_contract) {
       return push_action(  recover_contract_name, recover_abi_ser, recover_contract_name, N(init), mvo()
           ( "score_limit",               score_limit)
           ( "amax_proxy_contract",       amax_proxy_contract)
      );
   }

   action_result recover_action_addcontract(  name check_contract,
                                          asset cost, 
                                          string title,
                                          string desc, 
                                          string& url, 
                                          uint8_t score,
                                           name status ) {
      return push_action(  recover_contract_name, recover_abi_ser, recover_contract_name, N(addcontract), mvo()
           ( "check_contract",      check_contract)
           ( "cost",       cost)
           ( "title",      title)
           ( "desc",       desc)
           ( "url",        url)
           ( "score",      score)
           ( "status",     status)
      );
   }



   action_result recover_action_createorder(  
                        name admin,
                        name account,
                        recover_target_type recover_target,
                        bool manual_check_required)  {

         return push_action(  recover_contract_name, recover_abi_ser, recover_contract_name, N(createorder), mvo()
            ( "admin",              admin)
            ( "account",            account)
            ( "recover_target",      recover_target)
            ( "manual_check_required", manual_check_required)
      );                     
   
   }

   action_result recover_action_closeorder( const name& submitter, const uint64_t& order_id ) {
       return push_action(  recover_contract_name, recover_abi_ser, recover_contract_name, N(closeorder), mvo()
            ( "submitter",              submitter)
            ( "order_id",              order_id)
      );   
   }

   action_result proxy_action_init( name amax_recover ) {
       return push_action(  proxy_contract_name, proxy_abi_ser, proxy_contract_name, N(init), mvo()
            ( "amax_recover",              amax_recover)
      );   
   }

   action_result proxy_action_newaccount(const name& admin, const name& creator, const name& account, const authority& active) {
      return push_action(  proxy_contract_name, proxy_abi_ser, proxy_contract_name, N(init), mvo()
            ( "admin",              admin)
            ( "creator",            creator)
            ( "account",            account)
            ( "active",             active)
      );   
   }


   action_result checker_action_init( name contract, name amax_recover) {
      return push_action(  check1_contract_name, checker_abi_ser, check1_contract_name, N(init), mvo()
            ( "amax_recover",              amax_recover)
      );   
   }

};