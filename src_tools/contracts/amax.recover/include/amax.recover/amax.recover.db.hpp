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
    static constexpr eosio::name TELEGRAM   {"tg"_n };
    static constexpr eosio::name FACEBOOK   {"fb"_n };
}

// namespace ActionPermType {
//     //initial read-ID binding phase
//     static constexpr eosio::name BINDACCOUNT{"bindaccount"_n };
//     static constexpr eosio::name BINDANSWER {"bindanswer"_n };

//     //recovery phase
//     static constexpr eosio::name CREATEORDER{"createorder"_n };
//     static constexpr eosio::name CHKANSWER  {"chkanswer"_n };
//     static constexpr eosio::name CHKDID     {"chkdid"_n };
//     static constexpr eosio::name CHKMANUAL  {"chkmanual"_n };
// }

// namespace ManualCheckStatus {
//     static constexpr eosio::name SUCCESS    {"success"_n };
//     static constexpr eosio::name FAILURE    {"failure"_n };
// }

namespace ContractStatus {
    static constexpr eosio::name RUNNING    {"running"_n };
    static constexpr eosio::name STOPPED    {"stopped"_n };
}

namespace ContractAuditStatus {
    static constexpr eosio::name REGISTED    {"registed"_n };
    static constexpr eosio::name OPTIONAL    {"optional"_n };
    static constexpr eosio::name REQUIRED    {"required"_n };
}


namespace OrderStatus {
    static constexpr eosio::name CREATED        {"created"_n };
    static constexpr eosio::name FINISHED       {"finished"_n };
    static constexpr eosio::name CANCELLED      {"cancelled"_n };
}


/* public key -> update content */
typedef std::variant<eosio::public_key, string> recover_target_type;

NTBL("global") global_t {
    uint8_t                     recover_threshold;    //minimum score for recovery 
    uint64_t                    last_order_id;
    name                        amax_proxy_contract;

    EOSLIB_SERIALIZE( global_t, (recover_threshold)(last_order_id)(amax_proxy_contract))
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

//Scope: self
TBL account_audit_t {
    name                        account;    
    map<name, name>             audit_contracts;
    uint32_t                    threshold;  // >= global.recover_threshold, can be set by user
    time_point_sec              created_at;
    time_point_sec              recovered_at;                            

    account_audit_t() {}
    account_audit_t(const name& i): account(i) {}

    uint64_t primary_key()const { return account.value ; }

    typedef eosio::multi_index< "acctaudits"_n,  account_audit_t> idx;

    EOSLIB_SERIALIZE( account_audit_t, (account)(audit_contracts)(threshold)(created_at)(recovered_at) )
};

//Scope: self
TBL recover_order_t {
    uint64_t            id                  = 0;                    //PK
    uint64_t            sn                  = 0;                    //UK
    name                account;                                    //UK
    name                recover_type;
    map<name, uint8_t>  scores;                                     //contract -> score
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

    typedef eosio::multi_index
    < "orders"_n,  recover_order_t,
        indexed_by<"accountidx"_n, const_mem_fun<recover_order_t, uint64_t, &recover_order_t::by_account> >
    > idx_t;

    EOSLIB_SERIALIZE( recover_order_t,  (id)(sn)(account)(recover_type)
                                        (scores)(manual_check_required)(pay_status)
                                        (created_at)(expired_at)(updated_at)
                                        (recover_target))
};

struct audit_conf_s {
    asset           charge;
    string          title;
    string          desc;
    string          url;
    uint8_t         max_score;
    bool            required_check = false;
    name            status;
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
    bool            required_check = false;
    name            status;

    audit_conf_t() {}
    audit_conf_t(const name& contract): contract(contract) {}

    uint64_t primary_key()const { return contract.value; }
    uint64_t by_audit_type()const { return audit_type.value; }

    typedef eosio::multi_index< "auditconf"_n,  audit_conf_t,
        indexed_by<"audittype"_n, const_mem_fun<audit_conf_t, uint64_t, &audit_conf_t::by_audit_type>>
     > idx_t;

    EOSLIB_SERIALIZE( audit_conf_t, (contract)(audit_type)(charge)(title)(desc)(url)(max_score)(required_check)(status) )
};

} //namespace amax