#pragma once

#include <eosio/asset.hpp>
#include <eosio/eosio.hpp>
#include <eosio/permission.hpp>
#include <eosio/action.hpp>

#include <string>

#include <wasm_db.hpp>
#include<amax.system/native.hpp>
#include<amax.system/amax.system.hpp>


namespace amax {
using eosiosystem::authority;

class amax_proxy {
   public:
      [[eosio::action]] 
      void newaccount( const name& checker_contract, const name& creator, const name& account, const authority& active );

      using newaccount_action = eosio::action_wrapper<"newaccount"_n, &amax_proxy::newaccount>;
};


} //namespace amax
