 #pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include "wasm_db.hpp"
#include "utils.hpp"

// #include <deque>
#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>

namespace amax {

using namespace std;
using namespace eosio;

#define HASH256(str) sha256(str.c_str(), str.size())

#define TBL struct [[eosio::table, eosio::contract("amax.mulsign")]]

static constexpr name       SYS_BANK              = "amax.token"_n;
static constexpr symbol     SYS_SYMBOL            = symbol(symbol_code("AMAX"), 8);
static constexpr uint64_t   seconds_per_day       = 24 * 3600;


static constexpr uint16_t       MAX_TITLE_LENGTH = 64;
static constexpr uint16_t       MAX_CONTENT_LENGTH = 512;
static constexpr uint16_t       MAX_MULSIGNER_LENGTH = 100;

namespace proposal_status {
    static constexpr name PROPOSED = "proposed"_n;
    static constexpr name APPROVED = "approved"_n;
    static constexpr name EXECUTED = "executed"_n;
    static constexpr name CANCELED = "canceled"_n;
}

namespace proposal_type {
    static constexpr eosio::name setfee               = "setfee"_n;
    static constexpr eosio::name transfer             = "transfer"_n;
    static constexpr eosio::name setmulsignm          = "setmulsignm"_n;
    static constexpr eosio::name setmulsigner         = "setmulsigner"_n;
    static constexpr eosio::name delmulsigner         = "delmulsigner"_n;
    static constexpr eosio::name setproexpiry         = "setproexpiry"_n;
};

enum proposal_vote {
    PROPOSAL_AGAINST       = 0,
    PROPOSAL_FOR          = 1,
};

struct setfee_data {
    name issuer;
    uint64_t wallet_id;
    asset wallet_fee;
};

struct transfer_data {
    name from;
    name to;
    asset quantity;
    string memo;
};

struct setmulsigner_data {
    name issuer;
    uint64_t wallet_id;
    name mulsigner;
    uint32_t weight;
};

struct  setmulsignm_data{
    name issuer;
    uint64_t wallet_id;
    uint32_t mulsignm;
};

struct delmulsigner_data {
    name issuer;
    uint64_t wallet_id;
    name mulsigner;
};

struct setproexpiry_data {
    name issuer;
    uint64_t wallet_id;
    uint64_t expiry_sec;
};

struct [[eosio::table("global"), eosio::contract("amax.mulsign")]] global_t {
    name fee_collector;         // who creates fee wallet (id = 0)
    asset wallet_fee;
    set<name> support_contracts;
    EOSLIB_SERIALIZE( global_t, (fee_collector)(wallet_fee)(support_contracts) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

TBL wallet_t {
    uint64_t                id;
    string                  title;
    uint32_t                mulsign_m;
    uint32_t                mulsign_n;      // m <= n
    map<name, uint32_t>     mulsigners;     // mulsigner : weight
    map<extended_symbol, int64_t>    assets;         // symb@bank_contract  : amount
    uint64_t                proposal_expiry_sec = seconds_per_day * 7;
    name                    creator;
    time_point_sec          created_at;
    time_point_sec          updated_at;

    uint64_t primary_key()const { return id; }

    wallet_t() {}
    wallet_t(const uint64_t& wid): id(wid) {}

    uint64_t by_creator()const { return creator.value; }
    checksum256 by_title()const { return HASH256(title); }

    EOSLIB_SERIALIZE( wallet_t, (id)(title)(mulsign_m)(mulsign_n)(mulsigners)(assets)(proposal_expiry_sec)
                                (creator)(created_at)(updated_at) )

    typedef eosio::multi_index
    < "wallets"_n,  wallet_t,
        indexed_by<"creatoridx"_n, const_mem_fun<wallet_t, uint64_t, &wallet_t::by_creator> >,
        indexed_by<"titleidx"_n, const_mem_fun<wallet_t, checksum256, &wallet_t::by_title> >
    > idx_t;
};

TBL proposal_t {
    uint64_t            id;
    uint64_t            wallet_id;
    name                proposer;
    action              execution;
    string              excerpt;            //propose title
    string              description;        //propose detail, can be a text or url
    map<name,uint32_t>  approvers;          //updated in approve process
    uint32_t            recv_votes;         //received votes, based on mulsigner's weight
    time_point_sec      created_at;         //proposal expired after
    time_point_sec      expired_at;         //proposal expired after
    time_point_sec      updated_at;        //proposal executed after m/n approval
    name                status;

    uint64_t            primary_key()const { return id; }

    proposal_t() {}
    proposal_t(const uint64_t& pid): id(pid) {}

    uint64_t by_wallet_id()const { return wallet_id; }

    EOSLIB_SERIALIZE( proposal_t,   (id)(wallet_id)(proposer)(execution)(excerpt)(description)(approvers)
                                    (recv_votes)(created_at)(expired_at)(updated_at)(status) )

    typedef eosio::multi_index
    < "proposals"_n,  proposal_t,
        indexed_by<"walletidx"_n, const_mem_fun<proposal_t, uint64_t, &proposal_t::by_wallet_id> >
    > idx_t;
};

}