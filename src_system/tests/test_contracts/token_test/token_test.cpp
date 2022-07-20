#include "token_test.hpp"
#include <eosio/transaction.hpp>
#include <amax.token/amax.token.hpp>

using namespace eosio;

static constexpr eosio::name active_permission{"active"_n};
static constexpr eosio::name token_account{"amax.token"_n};

void token_test::testtransfer(const name &from,
                              const name &to,
                              const asset &quantity,
                              const string &memo)
{
   token::transfer_action transfer_act{ token_account, { {from, active_permission} } };
   transfer_act.send( from, to, quantity, memo );
}

void token_test::ontransfer(name from, name to, asset quantity, string memo) {
   if (memo == "test_ram_payer") {
      accounts accts( get_self(), get_self().value );
      auto from_acct = accts.find( from.value );
      if( from_acct == accts.end() ) {
         accts.emplace( from, [&]( auto& a ){
            a.acct = from;
         });
      } else {
         accts.modify( from_acct, from, [&]( auto& a ) {
            a.acct = from;
         });
      }
   }
}
