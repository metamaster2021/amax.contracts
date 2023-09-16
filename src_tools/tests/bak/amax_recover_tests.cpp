#include <boost/test/unit_test.hpp>
#include <eosio/testing/tester.hpp>
#include <eosio/chain/abi_serializer.hpp>
// #include "amax.system_tester.hpp"
#include "contracts.hpp"

#include "Runtime/Runtime.h"

#include <fc/variant_object.hpp>
#include "realme_dao_base_tester.hpp"

using namespace eosio::testing;
using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using namespace fc;
using namespace std;

using mvo = fc::mutable_variant_object;


class realme_dao_tester : public realme_dao_base_tester {
public:

   realme_dao_tester() {
      init_contract();
   }
   
   void init_contract() {
      create_accounts( { N(admin), N(account) } );

      action_init( 5 );
      // auto g = get_table_global();
      // REQUIRE_MATCHING_OBJECT( g, mvo()
      //    ("recover_threshold", 5)
      //    ("last_order_id", 0)
      // );
      produce_blocks(1);



      set<name> actions = {N(bindaccount), N(bindanswer), N(createorder), N(chkanswer), N(chkdid), N(chkmanual)};
      
      // action_setauth(N(admin), actions);
      produce_blocks(1);
   }
};

// BOOST_AUTO_TEST_SUITE(realme_dao)

// BOOST_FIXTURE_TEST_CASE( realme_dao_tests, realme_dao_tester ) try {

//    // get_table_auditscore("mobileno");
//    // wdump(("accaudit:")(accaudit));

//    //set account owner to realme.dao

//    std::cout<<"add code authority begin.\n";
//    auto account_owner_auth = get_auth(N(account), N(owner));
//    wdump(("---account owner auth: ---")(account_owner_auth));
//    // account_owner_auth.accounts.push_back(permission_level_weight{ {N(realme.dao), N(active)}, 1});
//    // sys_updateauth( N(account), N(owner), {}, account_owner_auth );
//    // std::cout<<"add code authority end.\n";
//    // produce_blocks(1);
//    // auto account_owner_auth2 = get_auth(N(account), N(owner));
//    // wdump(("---account owner auth2: ---")(account_owner_auth2));


//    // action_bindaccount(N(admin), N(account), "mobile_hash" );
//    // produce_blocks(1);
//    // std::cout << "action_bindaccount ---- end \n" ; 
//    // // auto accaudit = get_table_accaudits(N(account));
//    // // wdump(("accaudit:")(accaudit));

//    // map<uint8_t, string> answers = {{1, "answer1"}, {2, "answer2"}};
//    // action_bindanswer (N(admin), N(account), answers );
//    // produce_blocks(1);

//    // std::cout << "action_bindanswer ---- end\n"; 

//    // auto new_active_pubkey = get_public_key( N(account), "active_new" );
//    // wdump(("---new active pubkey ---")(new_active_pubkey));
   
//    // action_createorder( N(admin) , N(account), "mobile_hash", {"public_key", new_active_pubkey.to_string()}, false);
//    // std::cout << "action_createorder ---- end\n"; 
//    // produce_blocks(1);

//    // auto order = get_table_recoverorder(1);
//    // wdump(("order:")(order));

//    // action_chkanswer(N(admin), 1, N(account),2);
//    // std::cout << "action_chkanswer ---- end\n"; 
//    // produce_blocks(1);

//    // action_chkdid(N(admin), 1, N(account), true);
//    // std::cout << "action_chkdid ---- end\n"; 
//    // produce_blocks(1);


//    // action_chkmanual(N(admin), 1, N(account), true);
//    // std::cout << "action_chkmanual ---- end\n"; 
//    // produce_blocks(1);


//    // action_closeorder(N(account), 1);
//    // std::cout << "action_closeorder ---- end\n"; 
//    // produce_blocks(1);

//    // std::cout<<"add code authority begin.\n";
//    // auto contract_auth3 = get_auth(N(realme.dao), N(owner));
//    // wdump(("realme.dao ---new account auth3: ---")(contract_auth3));
   

//    std::cout<<"add code authority end.\n";


// } FC_LOG_AND_RETHROW()

// BOOST_AUTO_TEST_SUITE_END()
