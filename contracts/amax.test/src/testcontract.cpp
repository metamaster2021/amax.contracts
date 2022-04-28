#include <testcontract.hpp>

[[eosio::action]]
void testcontract::hi( name nm ) {
   print_f("Name : %\n", nm);
}

[[eosio::action]]
void testcontract::check( name nm ) {
   print_f("Name : %\n", nm);
   eosio::check(nm == "testcontract"_n, "check name not equal to `testcontract`");
}

// Checks the input param `nm` and returns serialized std::pair<int, std::string> instance.
[[eosio::action]]
std::pair<int, std::string> testcontract::checkwithrv( name nm ) {
   print_f("Name : %\n", nm);

   std::pair<int, std::string> results = {0, "NOP"};
   if (nm == "testcontract"_n) {
      results = {0, "Validation has passed."};
   }
   else {
      results = {1, "Input param `name` not equal to `testcontract`."};
   }
   return results; // the `set_action_return_value` intrinsic is invoked automatically here
}