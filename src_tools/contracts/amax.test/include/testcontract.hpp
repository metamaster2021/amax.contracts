#include <eosio/eosio.hpp>
using namespace eosio;

class [[eosio::contract("amax.test")]] testcontract: public eosio::contract {
   public:
      using contract::contract;

      [[eosio::action]] 
      void hi( name nm );

      [[eosio::action]] 
      void check( name nm );
      
      [[eosio::action]]
      std::pair<int, std::string> checkwithrv( name nm );

      using hi_action = action_wrapper<"hi"_n, &testcontract::hi>;
      using check_action = action_wrapper<"check"_n, &testcontract::check>;
      using checkwithrv_action = action_wrapper<"checkwithrv"_n, &testcontract::checkwithrv>;
};