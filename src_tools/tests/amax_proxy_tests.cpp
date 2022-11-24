#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
// #include "amax.system_tester.hpp"
#include "contracts.hpp"

#include "Runtime/Runtime.h"

#include <fc/variant_object.hpp>
#include "amax_proxy_base_tester.hpp"

using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;
using namespace std;

using mvo = fc::mutable_variant_object;


class amax_proxy_tester : public amax_proxy_base_tester {
public:

   amax_proxy_tester() {
      init_contract();
   }
   
   void init_contract() {
      create_accounts( { N(admin), N(account) } );

      // action_init( 5 );
      // auto g = get_table_global();
      // REQUIRE_MATCHING_OBJECT( g, mvo()
      //    ("score_limit", 5)
      //    ("last_order_id", 0)
      // );
      produce_blocks(1);


      set<name> actions = {N(bindaccount), N(bindanswer), N(createorder), N(chkanswer), N(chkdid), N(chkmanual)};
      
      // action_setauditor(N(admin), actions);
      produce_blocks(1);
   }
};

BOOST_AUTO_TEST_SUITE(amax_proxy_tests)

BOOST_FIXTURE_TEST_CASE( init_tests, amax_proxy_tester ) try {

   // get_table_auditscore("mobileno");
   // wdump(("accaudit:")(accaudit));

   //set account owner to amax.recover

   std::cout<<"add code authority begin.\n";
   auto account_owner_auth = get_auth(N(account), N(owner));
   wdump(("---account owner auth: ---")(account_owner_auth));
   account_owner_auth.accounts.push_back(permission_level_weight{ {global_contract_name, N(active)}, 1});
   sys_updateauth( N(account), N(owner), {}, account_owner_auth );
   std::cout<<"add code authority end.\n";
   produce_blocks(1);
   auto account_owner_auth2 = get_auth(N(account), N(owner));
   wdump(("---account owner auth2: ---")(account_owner_auth2));


   produce_blocks(1);
   std::cout << "action_bindaccount ---- end \n" ; 
   // auto accaudit = get_table_accaudits(N(account));
   // wdump(("accaudit:")(accaudit));

   map<uint8_t, string> answers = {{1, "answer1"}, {2, "answer2"}};
   produce_blocks(1);

   std::cout << "action_bindanswer ---- end\n"; 

   auto new_active_pubkey = get_public_key( N(account), "active_new" );
   wdump(("---new active pubkey ---")(new_active_pubkey));
   
   std::cout << "action_createorder ---- end\n"; 
   produce_blocks(1);

   auto order = get_table_recoverorder(1);
   wdump(("order:")(order));

   std::cout<<"add code authority end.\n";


} FC_LOG_AND_RETHROW()



BOOST_FIXTURE_TEST_CASE( update_pubkey_test, amax_proxy_tester ) try {

} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
