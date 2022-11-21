#pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

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


static constexpr eosio::name amax_account = "amax"_n;
static constexpr eosio::name owner = "owner"_n;


#define TBL struct [[eosio::table, eosio::contract("amax.recover")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("amax.recover")]]
namespace UpdateActionType {
    static constexpr eosio::name PUBKEY     {"pubkey"_n };
    static constexpr eosio::name MOBILENO   {"mobileno"_n };
    static constexpr eosio::name ANSWER     {"answer"_n };
}

namespace PayStatus {
    static constexpr eosio::name PAID       {"paid"_n };
    static constexpr eosio::name NOPAY      {"nopay"_n };
}

namespace AuditType {
    static constexpr eosio::name MOBILENO   {"mobileno"_n };
    static constexpr eosio::name ANSWER     {"answer"_n };
    static constexpr eosio::name DID        {"did"_n };
}

namespace ActionPermType {
    static constexpr eosio::name BINDACCOUNT{"bindaccount"_n };
    static constexpr eosio::name BINDANSWER{"bindanswer"_n };

    static constexpr eosio::name CREATEORDER{"createorder"_n };
    static constexpr eosio::name CHKANSWER  {"chkanswer"_n };
    static constexpr eosio::name CHKDID     {"chkdid"_n };
    static constexpr eosio::name CHKMANUAL  {"chkmanual"_n };
}

namespace ManualCheckStatus {
    static constexpr eosio::name SUCCESS    {"success"_n };
    static constexpr eosio::name FAILURE    {"failure"_n };
}

typedef std::variant<eosio::public_key, string> recover_target_type;

NTBL("global") global_t {
    uint8_t                     score_limit;
    uint64_t                    last_order_id;

    EOSLIB_SERIALIZE( global_t, (score_limit)(last_order_id))
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

TBL accountaudit_t {
    name                        account;            
    string                      mobile_hash;
    string                      answers;
    time_point_sec              created_at;

    accountaudit_t() {}
    accountaudit_t(const name& i): account(i) {}

    uint64_t primary_key()const { return account.value ; }

    typedef eosio::multi_index< "accaudits"_n,  accountaudit_t> idx;

    EOSLIB_SERIALIZE( accountaudit_t, (account)(mobile_hash)(answers)(created_at) )
};

//Scope: default
TBL recoverorder_t {
    uint64_t        id                   = 0;                   //PK
    name            account;                                    //UK
    name            recover_type;
    int8_t          mobile_check_score   = -1;        
    int8_t          answer_check_score   = -1;
    int8_t          did_check_score      = -1;
    bool            manual_check_required = false;
    name            manual_check_result; 
    name            manual_checker;        
    name            pay_status;
    time_point_sec  created_at;
    time_point_sec  expired_at;
    time_point_sec  updated_at;
    recover_target_type recover_target;                             //Eg: pubkey, mobileno


    recoverorder_t() {}
    recoverorder_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint64_t by_account() const { return account.value; }

    typedef eosio::multi_index
    < "orders"_n,  recoverorder_t,
        indexed_by<"accountidx"_n, const_mem_fun<recoverorder_t, uint64_t, &recoverorder_t::by_account> >
    > idx_t;

    EOSLIB_SERIALIZE( recoverorder_t, (id)(account)(recover_type)
                                     (mobile_check_score)
                                     (answer_check_score)(did_check_score)
                                     (manual_check_required)(manual_check_result)(manual_checker)
                                     (pay_status)(created_at)(expired_at)
                                     (updated_at)
                                     (recover_target))
};

TBL auditscore_t {
    name           audit_type;              //PK
    int8_t         score;         

    auditscore_t() {}
    auditscore_t(const name& type): audit_type(type) {}

    uint64_t primary_key()const { return audit_type.value; }

    typedef eosio::multi_index< "auditscores"_n,  auditscore_t > idx_t;

    EOSLIB_SERIALIZE( auditscore_t, (audit_type)(score) )
};


TBL auditor_t {
    name                    account;              //PK
    set<name>               actions;         

    auditor_t() {}
    auditor_t(const name& i): account(i) {}

    uint64_t primary_key()const { return account.value; }

    typedef eosio::multi_index< "auditors"_n,  auditor_t > idx_t;

    EOSLIB_SERIALIZE( auditor_t, (account)(actions) )
};

} //namespace amax