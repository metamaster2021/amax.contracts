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

static constexpr uint64_t seconds_per_day                   = 24 * 3600;
static constexpr uint64_t order_expiry_duration             = seconds_per_day;
static constexpr uint64_t manual_order_expiry_duration      = 3 * seconds_per_day;

#define TBL struct [[eosio::table, eosio::contract("realme.dao")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("realme.dao")]]
namespace UpdateActionType {
    static constexpr eosio::name PUBKEY     { "pubkey"_n    };
    static constexpr eosio::name MOBILENO   { "mobileno"_n  };
    static constexpr eosio::name ANSWER     { "answer"_n    };
}
namespace PayStatus {
    static constexpr eosio::name PAID       { "paid"_n      };
    static constexpr eosio::name NOPAY      { "nopay"_n     };
}
namespace RealmeCheckType {
    static constexpr eosio::name MOBILENO    { "mobileno"_n     };
    static constexpr eosio::name SAFETYANSWER{ "safetyanswer"_n };
    static constexpr eosio::name DID         { "did"_n          };
    static constexpr eosio::name MANUAL      { "manual"_n       };
    static constexpr eosio::name TELEGRAM    { "tg"_n           };
    static constexpr eosio::name FACEBOOK    { "fb"_n           };
    static constexpr eosio::name WHATSAPP    { "wa"_n           };
    static constexpr eosio::name MAIL        { "mail"_n         };
}
namespace ContractStatus {
    static constexpr eosio::name RUNNING     { "running"_n };
    static constexpr eosio::name STOPPED     { "stopped"_n };
}

namespace OrderStatus {
    static constexpr eosio::name PENDING     { "pending"_n   };
    static constexpr eosio::name FINISHED    { "finished"_n  };
    static constexpr eosio::name CANCELLED   { "cancelled"_n };
}


/* public key -> update content */
typedef std::variant<eosio::public_key, string> recover_target_type;

NTBL("global") global_t {
    uint8_t                     recover_threshold_pct = 70;    //minimum score for recovery 
    uint64_t                    last_order_id;
    name                        realme_owner_contract;

    EOSLIB_SERIALIZE( global_t, (recover_threshold_pct)(last_order_id)(realme_owner_contract))
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

//scope: account
TBL register_auth_t {
    name                        auth_contract;  //authorized: arm.didauth, arm.mblauth, arm.qaauth, ...
    time_point_sec              created_at;

    register_auth_t() {}
    register_auth_t(const name& i): auth_contract(i) {}

    uint64_t primary_key()const { return auth_contract.value ; }

    typedef eosio::multi_index< "regauths"_n,  register_auth_t> idx_t;

    EOSLIB_SERIALIZE( register_auth_t, (auth_contract)(created_at) )
};

//Scope: _self
TBL recover_auth_t {
    name                        account;    
    map<name, bool>             auth_requirements;     // contract -> bool: required | optional
    uint32_t                    recover_threshold;        // >= global.recover_threshold, can be set by user
    time_point_sec              created_at;
    time_point_sec              updated_at;
    time_point_sec              last_recovered_at;     

    recover_auth_t() {}
    recover_auth_t(const name& i): account(i) {}

    uint64_t primary_key()const { return account.value ; }

    typedef eosio::multi_index< "recauths"_n,  recover_auth_t> idx_t;

    EOSLIB_SERIALIZE( recover_auth_t, (account)(auth_requirements)(recover_threshold)(created_at)(updated_at)(last_recovered_at) )
};

//Scope: self
TBL recover_order_t {
    uint64_t            id                  = 0;                    //PK
    uint64_t            sn                  = 0;                    //UK
    name                account;                                    //UK
    name                recover_type;
    map<name, int8_t>  scores;                                     //contract -> score
    bool                manual_check_required = false;       
    name                pay_status;
    name                status;
    time_point_sec      created_at;
    time_point_sec      expired_at;
    time_point_sec      updated_at;
    recover_target_type recover_target;                             //Eg: pubkey, mobileno

    recover_order_t() {}
    recover_order_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint64_t by_account() const { return account.value; }
    uint64_t by_sn() const { return sn; }

    typedef eosio::multi_index
    < "recorders"_n,  recover_order_t,
        indexed_by<"accountidx"_n, const_mem_fun<recover_order_t, uint64_t, &recover_order_t::by_account> >,
        indexed_by<"snidx"_n, const_mem_fun<recover_order_t, uint64_t, &recover_order_t::by_sn> >
    > idx_t;

    EOSLIB_SERIALIZE( recover_order_t,  (id)(sn)(account)(recover_type)
                                        (scores)(manual_check_required)(pay_status)(status)
                                        (created_at)(expired_at)(updated_at)
                                        (recover_target))
};

struct audit_conf_s {
    asset           charge;
    string          title;
    string          desc;
    string          url;
    uint8_t         max_score;
    bool            check_required = false;
    name            status;
    bool            account_actived = false;
};

//Scope: self
TBL audit_conf_t {
    name            contract;
    name            audit_type;
    asset           charge;
    string          title;
    string          desc;
    string          url;
    uint8_t         max_score;
    bool            check_required = false;
    name            status;
    bool            account_actived = false;

    audit_conf_t() {}
    audit_conf_t(const name& contract): contract(contract) {}

    uint64_t primary_key()const { return contract.value; }
    uint64_t by_audit_type()const { return audit_type.value; }

    typedef eosio::multi_index< "auditconf"_n,  audit_conf_t,
        indexed_by<"audittype"_n, const_mem_fun<audit_conf_t, uint64_t, &audit_conf_t::by_audit_type>>
     > idx_t;

    EOSLIB_SERIALIZE( audit_conf_t, (contract)(audit_type)(charge)(title)
                    (desc)(url)(max_score)(check_required)(status)(account_actived) )
};

} //namespace amax