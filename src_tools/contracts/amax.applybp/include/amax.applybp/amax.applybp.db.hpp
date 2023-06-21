#pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>
#include <eosio/binary_extension.hpp> 
#include <utils.hpp>

#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>


namespace amax {

using namespace std;
using namespace eosio;

#define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size())

static constexpr uint32_t MAX_LOGO_SIZE        = 512;
static constexpr uint32_t MAX_TITLE_SIZE        = 2048;

#define TBL struct [[eosio::table, eosio::contract("amax.applybp")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("amax.applybp")]]

namespace ProducerStatus {
    static constexpr eosio::name DISABLE     { "disable"_n   };
    static constexpr eosio::name ENABLE      { "enable"_n  };
}

namespace ActionType {
    static constexpr eosio::name APPLYBP      { "newaccount"_n  }; //create a new account
    static constexpr eosio::name BINDINFO       { "bindinfo"_n    }; //bind audit info
    static constexpr eosio::name UPDATEINFO     { "updateinfo"_n    }; //bind audit info
    static constexpr eosio::name CREATECORDER   { "createorder"_n }; //create recover order
    static constexpr eosio::name SETSCORE       { "setscore"_n    }; //set score for user verfication
}

/* public key -> update content */
typedef std::variant<eosio::public_key, string> recover_target_type;

NTBL("global") global_t {
    name                     admin;   
    name                     dao_contract = "mdao.info"_n;

    EOSLIB_SERIALIZE( global_t, (admin)(dao_contract))
};

typedef eosio::singleton< "global"_n, global_t > global_singleton;

//scope: _self
TBL producer_t {
    name                        owner; 
    string                      logo_uri;       
    string                      org_name;                   // cn:xxx|en:xxX
    string                      org_info;                   // web:xxx|tw:xxx|tg:xxX
    name                        dao_code;                   
    uint32_t                    reward_shared_ratio = 0;         
    string                      manifesto;                 // cn:xxx|en:xxx
    string                      issuance_plan;              // cn:xxx|en:xxx
    string                      reward_shared_plan;         // cn:xxx|en:xxx
    name                        status;                     // disable | enable
    time_point_sec              created_at;
    time_point_sec              updated_at;
    // time_point_sec              last_edited_at;           
    
    producer_t() {}
    producer_t(const name& i): owner(i) {}

    uint64_t primary_key()const { return owner.value ; }

    typedef eosio::multi_index< "producers"_n,  producer_t> table;

    EOSLIB_SERIALIZE( producer_t, (owner)(logo_uri)(org_name)(org_info)
                                    (dao_code)(reward_shared_ratio)(manifesto)(issuance_plan)
                                    (reward_shared_plan)(status)
                                    (created_at)(updated_at))
};

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