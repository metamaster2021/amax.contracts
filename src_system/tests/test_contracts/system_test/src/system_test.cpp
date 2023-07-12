#include <amax.system/amax.system.hpp>
#include <system_test/system_test.hpp>

namespace amax_system {

#define CHECK(exp, msg) { if (!(exp)) eosio::check(false, msg); }
#ifndef ASSERT
    #define ASSERT(exp) eosio::CHECK(exp, #exp)
#endif

static constexpr eosio::name amax_account = "amax"_n;

void system_test::updateauthex( const name& actor,
                                const name& required_permission,
                                const name& account,
                                const name& permission,
                                const name& parent,
                                const authority& auth ) {
    require_auth(actor);
    eosiosystem::system_contract::updateauth_action act(amax_account, { {account, required_permission} });
    act.send( account, permission, parent, auth);

}

[[eosio::action]]
void system_test::transferex(   const name& actor,
                                const name& required_permission,
                                const name& from,
                                const name& to,
                                const asset& quantity,
                                const std::string& memo ) {
    require_auth(actor);
    token::transfer_action act("amax.token"_n, {{ from, required_permission }});
    act.send(from, to, quantity, memo);
}


} /// namespace amax_xtoken
