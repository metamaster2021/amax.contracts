#pragma once

#include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <eosio/singleton.hpp>
#include <eosio/system.hpp>
#include <eosio/time.hpp>

// #include <deque>
#include <optional>
#include <string>
#include <map>
#include <set>
#include <type_traits>

namespace amax {

using namespace std;
using namespace eosio;

#define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size())
#define TBL struct [[eosio::table, eosio::contract("amax.ntoken")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("amax.ntoken")]]

NTBL("global") global_t {
    set<name> notaries;

    EOSLIB_SERIALIZE( global_t, (notaries) )
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;

struct nsymbol {
    uint32_t id;
    uint32_t parent_id;

    nsymbol() {}
    nsymbol(const uint32_t& i): id(i),parent_id(0) {}
    nsymbol(const uint32_t& i, const uint32_t& pid): id(i),parent_id(pid) {}

    friend bool operator==(const nsymbol&, const nsymbol&);
    bool is_valid()const { return( id > parent_id ); }
    uint64_t raw()const { return( (uint64_t) parent_id << 32 | id ); } 

    EOSLIB_SERIALIZE( nsymbol, (id)(parent_id) )
};

bool operator==(const nsymbol& symb1, const nsymbol& symb2) { 
    return( symb1.id == symb2.id && symb1.parent_id == symb2.parent_id ); 
}


///Scope: symbole.id
struct nasset {
    int64_t         amount;
    nsymbol         symbol;

    nasset() {}
    nasset(const uint32_t& id): symbol(id), amount(0) {}
    nasset(const uint32_t& id, const uint32_t& pid): symbol(id, pid), amount(0) {}
    nasset(const uint32_t& id, const uint32_t& pid, const int64_t& am): symbol(id, pid), amount(am) {}

    nasset& operator+=(const nasset& quantity) { 
        check( quantity.symbol.raw() == this->symbol.raw(), "nsymbol mismatch");
        this->amount += quantity.amount; return *this;
    } 
    nasset& operator-=(const nasset& quantity) { 
        check( quantity.symbol.raw() == this->symbol.raw(), "nsymbol mismatch");
        this->amount -= quantity.amount; return *this; 
    }

    bool is_valid()const { return symbol.is_valid(); }
    
    EOSLIB_SERIALIZE( nasset, (amount)(symbol) )
};

TBL nstats_t {
    nasset          supply;
    nasset          max_supply;     // 1 means NFT-721 type
    string          token_uri;      // globally unique uri for token metadata { image, desc,..etc }
    name            ipowner;        // who owns the IP
    name            notary;         // who notarized the IP authenticity and owership
    name            issuer;         // who created/uploaded/issued this NFT
    time_point_sec  issued_at;
    time_point_sec  notarized_at;
    bool            paused;

    nstats_t() {};
    nstats_t(const uint64_t& id): supply(id) {};
    nstats_t(const uint64_t& id, const uint64_t& pid): supply(id, pid) {};
    nstats_t(const uint64_t& id, const uint64_t& pid, const int64_t& am): supply(id, pid, am) {};

    uint64_t primary_key()const     { return supply.symbol.id; } // must use id to keep available_primary_key increase consistenly
    uint64_t by_parent_id()const    { return supply.symbol.parent_id; }
    uint64_t by_ipowner()const      { return ipowner.value; }
    uint64_t by_issuer()const       { return issuer.value; }
    checksum256 by_token_uri()const { return HASH256(token_uri); } // unique index

    typedef eosio::multi_index
    < "tokenstats"_n,  nstats_t,
        indexed_by<"parentidx"_n,  const_mem_fun<nstats_t, uint64_t, &nstats_t::by_parent_id> >,
        indexed_by<"ipowneridx"_n, const_mem_fun<nstats_t, uint64_t, &nstats_t::by_ipowner> >,
        indexed_by<"issueridx"_n, const_mem_fun<nstats_t, uint64_t, &nstats_t::by_issuer> >,
        indexed_by<"tokenuriidx"_n, const_mem_fun<nstats_t, checksum256, &nstats_t::by_token_uri> >
    > idx_t;

    EOSLIB_SERIALIZE(nstats_t,  (supply)(max_supply)(token_uri)
                                (ipowner)(notary)(issuer)(issued_at)(notarized_at)(paused) )
};

///Scope: owner's account
TBL account_t {
    nasset      balance;
    bool        paused = false;   //if true, it can no longer be transferred

    account_t() {}
    account_t(const nasset& asset): balance(asset) {}

    uint64_t primary_key()const { return balance.symbol.raw(); }

    EOSLIB_SERIALIZE(account_t, (balance)(paused) )

    typedef eosio::multi_index< "accounts"_n, account_t > idx_t;
};

} //namespace amax