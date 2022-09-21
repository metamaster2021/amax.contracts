#pragma once

#include <eosio/chain/symbol.hpp>

#include <fc/log/logger.hpp>

#define CORE_SYM_NAME "TST"
#define CORE_SYM_PRECISION 4

#define _STRINGIZE1(x) #x
#define _STRINGIZE2(x) _STRINGIZE1(x)

#define CORE_SYM_STR ( _STRINGIZE2(CORE_SYM_PRECISION) "," CORE_SYM_NAME )
#define CORE_SYM  ( ::eosio::chain::string_to_symbol_c( CORE_SYM_PRECISION, CORE_SYM_NAME ) )
#undef CORE_SYMBOL
#define CORE_SYMBOL  ( ::eosio::chain::symbol( CORE_SYM_PRECISION, CORE_SYM_NAME ) )
#define CORE_ASSET(amount) ( eosio::chain::asset(amount, CORE_SYMBOL) )

namespace core_sym {
   static inline eosio::chain::asset from_string(const std::string& s) {
     return eosio::chain::asset::from_string(s + " " CORE_SYM_NAME);
   }

   static const eosio::chain::asset min_activated_stake = CORE_ASSET(50'000'000'0000'0000);
};
