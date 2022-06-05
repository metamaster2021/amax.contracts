
#include <amax.token.hpp>
#include "bootdao.hpp"

#include <amax.system/amax.system.hpp>

#include <chrono>

using namespace wasm;
using namespace eosiosystem;

static constexpr name AMAX_BANK  = "amax.token"_n;
static constexpr symbol   AMAX   = symbol(symbol_code("AMAX"), 8);

static constexpr eosio::name active_permission{"active"_n};
using undelegatebw_action = eosio::action_wrapper<"undelegatebw"_n, &system_contract::undelegatebw>;

// transfer out from contract self
#define TRANSFER_OUT(token_contract, to, quantity, memo) \
            token::transfer_action(token_contract, {{get_self(), active_permission}}) \
            .send( get_self(), to, quantity, memo);

#define UNDELEGATE_BW(from, receiver, unstate_net, unstate_cpu) \
            system_contract::undelegatebw_action( "amax"_n, {{get_self(), active_permission}}) \
            .send(from, receiver, unstate_net, unstate_cpu);

#define SELL_RAM(from, rambytes) \
            system_contract::sellram_action( "amax"_n, {{get_self(), active_permission}}) \
            .send(from, rambytes);

[[eosio::action]]
void bootdao::init() {
    auto& acct = _gstate.whitelist_accounts;
    acct.insert("amax.custody"_n);
    acct.insert("armoniaadmin"_n);
    acct.insert("amax.satoshi"_n);
    acct.insert("dragonmaster"_n);
    acct.insert("armonia1"_n);
    acct.insert("armonia2"_n);
    acct.insert("armonia3"_n);
    acct.insert("amax.ram"_n);
    acct.insert("amax.stake"_n);
    acct.insert("masteraychen"_n);
}

void bootdao::recycle(const vector<name>& accounts) {
    require_auth(get_self());

    for( auto& account: accounts ){
        recycle_account( account );
    }
}

void bootdao::recycle_account(const name& account) {
    check( _gstate.whitelist_accounts.find(account) == _gstate.whitelist_accounts.end(), "whitelisted" );

    user_resources_table  userres( "amax"_n, account.value );
    auto res_itr = userres.find( account.value );
    check( res_itr !=  userres.end(), "account res not found: " + account.to_string() );

    auto net_bw = asset(200000, SYS_SYMB);
    auto cpu_bw = asset(200000, SYS_SYMB);
    if ( res_itr->net_weight > net_bw ) {
        //undelegate net
        net_bw = res_itr->net_weight - net_bw;
    } else 
        net_bw.amount = 0;

    if( res_itr->cpu_weight > cpu_bw ){
        //undelegate cpu
        cpu_bw = res_itr->cpu_weight - cpu_bw;
    } else 
        cpu_bw.amount = 0;

    //undelegate net or cpu
    if( net_bw.amount > 0 || cpu_bw.amount > 0 )
        UNDELEGATE_BW( account, "amax"_n, net_bw, cpu_bw )

    //sell RAM
    auto rambytes = res_itr->ram_bytes;
    if( rambytes > 4000 )
        SELL_RAM( account, rambytes - 4000 )

    //reverse amax from balance
    auto amax_bal = get_balance(SYS_BANK, AMAX, account);
    if (amax_bal.amount >= 1000'0000) {
        TRANSFER( AMAX_BANK, "amax"_n, amax_bal, "" )
    }
    
}

asset bootdao::get_balance(const name& bank, const symbol& symb, const name& account) {
    tbl_accounts tmp(bank, account.value);
    auto itr = tmp.find(symb.code().raw());

    if (itr != tmp.end())
        return itr->balance;
    else 
        return asset(0, symb);
}
