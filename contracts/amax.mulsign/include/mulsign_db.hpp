 #pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

#include "wasm_db.hpp"

// #include <deque>
#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>

namespace amax {

using namespace std;
using namespace eosio;

#define TBL struct [[eosio::table, eosio::contract("amax.mulsign")]]

TBL wallet_t {
    uint64_t                id;
    uint8_t                 mulsign_m; 
    uint8_t                 mulsign_n;      // m <= n     
    map<name, uint8_t>      mulsigners;     // mulsigner : weight
    map<extended_symbol, int64_t> assets;   // ext_symb  : amount
    name                    creator;
    time_point_sec          created_at;
    time_point_sec          updated_at;

    uint64_t primary_key()const { return id; }

    wallet_t() {}
    wallet_t(const uint64_t& wid): id(wid) {}
    wallet_t(const uint64_t& wid, const uint8_t& m, const uint8_t& n): id(wid), mulsign_m(m), mulsign_n(n) {}

    uint64_t by_creator()const { return creator.value; }

    EOSLIB_SERIALIZE( wallet_t, (id)(mulsign_m)(mulsign_n)(mulsigners)(assets)
                                (creator)(created_at)(updated_at) )

    typedef eosio::multi_index
    < "wallets"_n,  wallet_t,
        indexed_by<"creatoridx"_n, const_mem_fun<wallet_t, uint64_t, &wallet_t::by_creator> >
    > idx_t;
};

TBL proposal_t {
    uint64_t            id;
    uint64_t            wallet_id;
    extended_asset      quantity;
    name                recipient;
    name                proposer;
    set<name>           approvers;          //updated in approve process
    uint8_t             recv_votes;         //received votes, based on mulsigner's weight
    time_point_sec      created_at;         //proposal expired after
    time_point_sec      expired_at;         //proposal expired after
    time_point_sec      executed_at;        //proposal executed after m/n approval

    uint64_t            primary_key()const { return id; }

    proposal_t() {}
    proposal_t(const uint64_t& pid): id(pid) {}

    uint64_t by_wallet_id()const { return wallet_id; }

    EOSLIB_SERIALIZE( proposal_t,   (id)(wallet_id)(quantity)(recipient)(proposer)(approvers)
                                    (recv_votes)(created_at)(expired_at)(executed_at) )

    typedef eosio::multi_index
    < "proposals"_n,  proposal_t,
        indexed_by<"walletidx"_n, const_mem_fun<proposal_t, uint64_t, &proposal_t::by_wallet_id> >
    > idx_t;

};

}