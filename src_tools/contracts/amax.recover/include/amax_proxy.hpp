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

class amax_proxy {
   public:
      [[eosio::action]] 
      void updateauth( const name& account, const eosio::public_key& pubkey );

      using updateauth_action = eosio::action_wrapper<"updateauth"_n, &amax_proxy::updateauth>;
};


} //namespace amax
