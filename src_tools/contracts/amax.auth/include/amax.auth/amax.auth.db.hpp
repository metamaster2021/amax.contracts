#pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>


#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>


namespace amax {

using namespace std;
using namespace eosio;

namespace ActionType {
    static constexpr eosio::name NEWACCOUNT     { "newaccount"_n  }; //create a new account
    static constexpr eosio::name BINDINFO       { "bindinfo"_n    }; //bind audit info
    static constexpr eosio::name CREATECORDER   { "createorder"_n }; //create recover order
    static constexpr eosio::name SETSCORE       { "setscore"_n    }; //set score for user verfication
}
static constexpr uint32_t MAX_TITLE_SIZE        = 256;

#define TBL struct [[eosio::table, eosio::contract("amax.auth")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("amax.auth")]]

namespace RealmeCheckType {
    static constexpr eosio::name MOBILENO       { "mobileno"_n     };
    static constexpr eosio::name SAFETYANSWER   { "safetyanswer"_n };
    static constexpr eosio::name DID            { "did"_n          };
    static constexpr eosio::name MANUAL         { "manual"_n       };
    static constexpr eosio::name TELEGRAM       { "tg"_n           };
    static constexpr eosio::name FACEBOOK       { "fb"_n           };
}

NTBL("global") global_t {
    name                        amax_recover_contract;
    name                        amax_proxy_contract;
    name                        auth_type;   //E.g. RealmeCheckType::MOBILENO

    EOSLIB_SERIALIZE( global_t, (amax_recover_contract)(amax_proxy_contract)(auth_type))
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

//Scope: _self 
TBL account_realme_t {
    name                        account;        //PK
    string                      realme_info;    //value: md5(md5(RM + salt)
    time_point_sec              created_at;

    account_realme_t() {}
    account_realme_t(const name& i): account(i) {}

    uint64_t primary_key()const { return account.value ; }

    typedef eosio::multi_index< "acctrealme"_n,  account_realme_t> idx_t;

    EOSLIB_SERIALIZE( account_realme_t, (account)(realme_info)(created_at) )
};

//Scope: _self
TBL auth_t {
    name                        auth;              //PK
    set<name>                   actions;              //set of action types

    auth_t() {}
    auth_t(const name& i): auth(i) {}

    uint64_t primary_key()const { return auth.value; }

    typedef eosio::multi_index< "auths"_n,  auth_t > idx_t;

    EOSLIB_SERIALIZE( auth_t, (auth)(actions) )
};

} //namespace amax