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
static auto check_mobile_contract_name  =  N(chk.mobile);
static auto check_answer_contract_name  =  N(chk.answer);

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
   abi_serializer auth_abi_ser;

   amax_proxy_base_tester() {

      initialize_proxy_contract( proxy_contract_name );

      initialize_recover_contract( recover_contract_name);


      initialize_auth_contract( check_mobile_contract_name);
      initialize_auth_contract( check_answer_contract_name);
      produce_blocks( 2 );

      initialize_abi_ser(check_mobile_contract_name, auth_abi_ser);
      produce_blocks( 2 );

      auth_action_init(check_mobile_contract_name, recover_contract_name);
      produce_blocks( 2 );

      auth_action_init(check_answer_contract_name, recover_contract_name);
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

   void initialize_auth_contract( name contract_name) {
      initialize_contract(contract_name, contracts::auth_wasm(), contracts::auth_abi());
   }

   void initialize_contract( name contract_name, std::vector<uint8_t> wasm_file, std::vector<char> abi_file ) {

       produce_blocks( 2 );

      create_accounts( { contract_name });
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
//       return get_table_common("recover_order_t", N(orders), name(order_id) );
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

   action_result recover_action_init( uint8_t recover_threshold, 
                                    name amax_proxy_contract) {
       return push_action(  recover_contract_name, recover_abi_ser, recover_contract_name, N(init), mvo()
           ( "recover_threshold",               recover_threshold)
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
      return push_action(  recover_contract_name, recover_abi_ser, recover_contract_name, N(addauditconf), mvo()
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
      std::cout << "proxy_action_init :" << proxy_contract_name << "," << amax_recover <<"---- end\n"; 

       return push_action(  proxy_contract_name, proxy_abi_ser, proxy_contract_name, N(init), mvo()
            ( "amax_recover",              amax_recover)
      );   
   }

   action_result proxy_action_newaccount(const name& admin, const name& creator, const name& account, const authority& active) {
      std::cout << "proxy_action_newaccount :" << proxy_contract_name << "," << account <<"---- start\n"; 

      return push_action(  proxy_contract_name, proxy_abi_ser, proxy_contract_name, N(newaccount), mvo()
            ( "admin",              admin)
            ( "creator",            creator)
            ( "account",            account)
            ( "active",             active)
      );   
   }


   action_result auth_action_init( name check_contract, name amax_recover) {
      std::cout << "auth_action_init :" << check_contract << "," << amax_recover <<"---- end\n"; 

      return push_action(  check_contract, auth_abi_ser, check_contract, N(init), mvo()
            ( "amax_recover",              amax_recover)
      );   
   }

   action_result auth_action_bindinfo (  name auth_contract, const name& admin, const name& account, const string& info) {
       return push_action(  auth_contract, auth_abi_ser, auth_contract, N(bindinfo), mvo()
            ( "auth_contract",               auth_contract)
            ( "admin",                          admin)
            ( "account",                        account)
            ( "info",                           info)
      );  
   }

   action_result auth_action_createcorder (  
                        name auth_contract,
                        const name& admin,
                        const name& account,
                        const recover_target_type& recover_target,
                        const bool& manual_check_required,
                        const uint8_t& score) {
       return push_action( auth_contract, auth_abi_ser, auth_contract, N(bindinfo), mvo()
            ( "admin",                 admin)
            ( "account",               account)
            ( "recover_target",        recover_target)
            ( "manual_check_required", manual_check_required)
            ( "score",                 score)
      );  
   }

   action_result auth_action_setscore(
                                 const name& auth_contract,
                                 const name& admin,
                                 const name& account,
                                 const uint64_t& order_id,
                                 const uint8_t& score ) {
       return push_action( auth_contract, auth_abi_ser, auth_contract, N(bindinfo), mvo()
            ( "admin",                 admin)
            ( "account",               account)
            ( "order_id",              order_id)
            ( "score",                 score)
      );  
   }

};