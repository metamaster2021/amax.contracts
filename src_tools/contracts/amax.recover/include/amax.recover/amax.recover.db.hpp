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
    static constexpr eosio::name MANUAL     {"manual"_n };
    static constexpr eosio::name TG         {"tg"_n };
    static constexpr eosio::name FACEBOOK    {"facebook"_n };
}

namespace ActionPermType {
    //initial read-ID binding phase
    static constexpr eosio::name BINDACCOUNT{"bindaccount"_n };
    static constexpr eosio::name BINDANSWER {"bindanswer"_n };

    //recovery phase
    static constexpr eosio::name CREATEORDER{"createorder"_n };
    static constexpr eosio::name CHKANSWER  {"chkanswer"_n };
    static constexpr eosio::name CHKDID     {"chkdid"_n };
    static constexpr eosio::name CHKMANUAL  {"chkmanual"_n };
}

namespace ManualCheckStatus {
    static constexpr eosio::name SUCCESS    {"success"_n };
    static constexpr eosio::name FAILURE    {"failure"_n };
}

namespace ContractStatus {
    static constexpr eosio::name RUNNING    {"running"_n };
    static constexpr eosio::name STOPPED    {"stopped"_n };
}

namespace ContractAuditStatus {
    static constexpr eosio::name REGISTED    {"registed"_n };
    static constexpr eosio::name OPTIONAL    {"optional"_n };
    static constexpr eosio::name REQUIRED    {"required"_n };
}

typedef std::variant<eosio::public_key, string> recover_target_type;

NTBL("global") global_t {
    uint8_t                     score_limit;
    uint64_t                    last_order_id;
    name                        amax_proxy_contract;

    EOSLIB_SERIALIZE( global_t, (score_limit)(last_order_id)(amax_proxy_contract))
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

TBL account_audit_t {
    name                        account;    
    map<name, name>             audit_contracts;
    uint32_t                    threshold;
    time_point_sec              created_at;
    time_point_sec              recovered_at;                            

    account_audit_t() {}
    account_audit_t(const name& i): account(i) {}

    uint64_t primary_key()const { return account.value ; }

    typedef eosio::multi_index< "accaudits"_n,  account_audit_t> idx;

    EOSLIB_SERIALIZE( account_audit_t, (account)(audit_contracts)(threshold)(created_at)(recovered_at) )
};

//Scope: default
TBL recover_order_t {
    uint64_t            id                   = 0;                   //PK
    uint64_t            serial_num           = 0;                   //UK
    name                account;                                    //UK
    name                recover_type;
    map<name, uint8_t>  scores;                                     //contract
    bool                manual_check_required = false;       
    name                pay_status;
    time_point_sec      created_at;
    time_point_sec      expired_at;
    time_point_sec      updated_at;
    recover_target_type recover_target;                             //Eg: pubkey, mobileno

    recover_order_t() {}
    recover_order_t(const uint64_t& i): id(i) {}

    uint64_t primary_key()const { return id; }
    uint64_t by_account() const { return account.value; }

    typedef eosio::multi_index
    < "orders"_n,  recover_order_t,
        indexed_by<"accountidx"_n, const_mem_fun<recover_order_t, uint64_t, &recover_order_t::by_account> >
    > idx_t;

    EOSLIB_SERIALIZE( recover_order_t, (id)(serial_num)(account)(recover_type)
                                     (scores)
                                     (manual_check_required)
                                     (pay_status)(created_at)(expired_at)
                                     (updated_at)
                                     (recover_target))
};

TBL audit_score_t {
    name            contract;
    name            audit_type;
    asset           cost;
    string          title;
    string          desc;
    string          url;
    uint8_t         score;
    bool            required_check = false;
    name            status;

    audit_score_t() {}
    audit_score_t(const name& contract): contract(contract) {}

    uint64_t primary_key()const { return contract.value; }
    uint64_t by_audit_type()const { return audit_type.value; }

    typedef eosio::multi_index< "auditscores"_n,  audit_score_t,
        indexed_by<"audittype"_n, const_mem_fun<audit_score_t, uint64_t, &audit_score_t::by_audit_type>>
     > idx_t;

    EOSLIB_SERIALIZE( audit_score_t, (contract)(audit_type)(cost)(title)(desc)(url)(score)(required_check)(status) )
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