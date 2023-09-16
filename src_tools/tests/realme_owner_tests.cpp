#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
// #include "amax.system_tester.hpp"
#include "contracts.hpp"

#include "Runtime/Runtime.h"

#include <fc/variant_object.hpp>
#include "realme_owner_base_tester.hpp"

using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;
using namespace std;

using mvo = fc::mutable_variant_object;
static auto p1_name= N(p1);


class realme_owner_tester : public realme_owner_base_tester {
public:
   realme_owner_tester() {
      init_contract();
   }
   
   void init_contract() {
      create_p1_account();


      // action_init( 5 );
      // auto g = get_table_global();
      // REQUIRE_MATCHING_OBJECT( g, mvo()
      //    ("recover_threshold", 5)
      //    ("last_order_id", 0)
      // );
      produce_blocks(1);

      // const name& admin, const name& creator, const name& account, const authority& active

      // action_newaccount(N(admin), global_contract_name, )

      // set<name> actions = {N(bindaccount), N(bindanswer), N(createorder), N(chkanswer), N(chkdid), N(chkmanual)};
      
      // // action_setchecker(N(admin), actions);
      // produce_blocks(1);
   }


   void create_p1_account() {
      std::cout<< "create_proxy_account -- begin" << std::endl;
      auto new_active_pubkey = authority(get_public_key( p1_name, "active_new" ));
      wdump(("---new active pubkey ---")(new_active_pubkey));
      proxy_action_newaccount(proxy_contract_name, proxy_contract_name, p1_name, new_active_pubkey );
      produce_blocks(1);

   }
};

BOOST_AUTO_TEST_SUITE(realme_owner_tests)

BOOST_FIXTURE_TEST_CASE( init_tests, realme_owner_tester ) try {

   // get_table_auditscore("mobileno");
   // wdump(("accaudit:")(accaudit));

   //set account owner to realme.dao

   // std::cout<<"add code authority begin.\n";
   // auto account_owner_auth = get_auth(N(account), N(owner));
   // wdump(("---account owner auth: ---")(account_owner_auth));
   // account_owner_auth.accounts.push_back(permission_level_weight{ {global_contract_name, N(active)}, 1});
   // sys_updateauth( N(account), N(owner), {}, account_owner_auth );
   // std::cout<<"add code authority end.\n";
   // produce_blocks(1);
   // auto account_owner_auth2 = get_auth(N(account), N(owner));
   // wdump(("---account owner auth2: ---")(account_owner_auth2));


   // produce_blocks(1);
   // std::cout << "action_bindaccount ---- end \n" ; 
   // // auto accaudit = get_table_accaudits(N(account));
   // // wdump(("accaudit:")(accaudit));

   // map<uint8_t, string> answers = {{1, "answer1"}, {2, "answer2"}};
   // produce_blocks(1);

   // std::cout << "action_bindanswer ---- end\n"; 

   // auto new_active_pubkey = get_public_key( N(account), "active_new" );
   // wdump(("---new active pubkey ---")(new_active_pubkey));
   
   // std::cout << "action_createorder ---- end\n"; 
   // produce_blocks(1);

   // auto order = get_table_recoverorder(1);
   // wdump(("order:")(order));

   std::cout<<"add code authority end.\n";


} FC_LOG_AND_RETHROW()


BOOST_AUTO_TEST_SUITE_END()
