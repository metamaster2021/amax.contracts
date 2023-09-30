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
#define SYMBOL(sym_code, precision) symbol(symbol_code(sym_code), precision)

#define hash(str) sha256(const_cast<char*>(str.c_str()), str.size())
static constexpr eosio::symbol SYS_SYMB         = SYMBOL("AMAX", 8);
static constexpr eosio::name SYS_CONTRACT       = "amax"_n;
static constexpr eosio::name OWNER_PERM         = "owner"_n;
static constexpr eosio::name ACTIVE_PERM        = "active"_n;

#define TBL struct [[eosio::table, eosio::contract("xchain.owner")]]
#define NTBL(name) struct [[eosio::table(name), eosio::contract("xchain.owner")]]

NTBL("global") global_t {
    name        admin                   = "armoniaadmin"_n;
    set<name>   oracle_makers;
    set<name>   oracle_checkers;
    uint64_t    ram_bytes               = 5000;
    asset       stake_net_quantity      = asset(10000, SYS_SYMB); //TODO: set params in global table
    asset       stake_cpu_quantity      = asset(40000, SYS_SYMB);
    EOSLIB_SERIALIZE( global_t, (admin)(ram_bytes)(stake_net_quantity)(stake_cpu_quantity))
};
typedef eosio::singleton< "global"_n, global_t > global_singleton;


namespace ChainType {
    static constexpr eosio::name BTC            { "btc"_n       };
    static constexpr eosio::name ETH            { "eth"_n       };
    static constexpr eosio::name BSC            { "bsc"_n       };
    static constexpr eosio::name TRON           { "tron"_n      };
};

namespace BindStatus {
    static constexpr eosio::name REQUESTED      { "requested"_n };
    static constexpr eosio::name APPROVED       { "approved"_n  }; //delete upon disapprove
};

//Scope: xchain, E.g. btc, eth, bsc, tron
TBL xchain_account_t {
    name                account;                //PK
    string              xchain_pubkey;          //UK: hash(xchain_pubkey)
    string              xchain_txid;            //UK: hash(txid)
    eosio::public_key   pubkey;                 //AMAX pubkey
    checksum256         amax_txid;
    name                bind_status;            //requested, approved
    time_point_sec      created_at;

    xchain_account_t() {}
    xchain_account_t( const name& a ): account(a) {}

    uint64_t primary_key()const { return account.value ; }

    eosio::checksum256 by_xchain_pubkey() const  { return hash(xchain_pubkey);  } 
    eosio::checksum256 by_xchain_txid()   const  { return hash(xchain_txid);            }
    typedef eosio::multi_index
    < "xchainaccts"_n,  xchain_account_t,
        indexed_by<"xchainpubkey"_n,    const_mem_fun<xchain_account_t, eosio::checksum256, &xchain_account_t::by_xchain_pubkey>>,
        indexed_by<"xchaintxid"_n,      const_mem_fun<xchain_account_t, eosio::checksum256, &xchain_account_t::by_xchain_txid>>
    > idx_t;

    EOSLIB_SERIALIZE( xchain_account_t, (account)(xchain_pubkey)(xchain_txid)(pubkey)(amax_txid)(bind_status)(created_at) )
};

//Scope: xchain, E.g. btc, eth, bsc, tron
TBL pubkey_update_log_t {
    name                account;                //PK
    string              xchain_pubkey;          //UK: hash(xchain_pubkey)
    string              xchain_txid;            //UK: hash(txid)
    name                action;                 //updatepub, addpub
    eosio::public_key   pubkey;                 //AMAX pubkey
    name                bind_status;            //requested, approved
    time_point_sec      created_at;

    pubkey_update_log_t() {}
    pubkey_update_log_t( const name& a ): account(a) {}

    uint64_t primary_key()const { return account.value ; }

    eosio::checksum256 by_xchain_pubkey() const  { return hash(xchain_pubkey);  } 
    eosio::checksum256 by_xchain_txid()   const  { return hash(xchain_txid);            }
    typedef eosio::multi_index
    < "pkupdatelogs"_n,  pubkey_update_log_t,
        indexed_by<"xchainpubkey"_n,    const_mem_fun<pubkey_update_log_t, eosio::checksum256, &pubkey_update_log_t::by_xchain_pubkey>>,
        indexed_by<"xchaintxid"_n,      const_mem_fun<pubkey_update_log_t, eosio::checksum256, &pubkey_update_log_t::by_xchain_txid>>
    > idx_t;

    EOSLIB_SERIALIZE( pubkey_update_log_t, (account)(xchain_pubkey)(xchain_txid)(action)(pubkey)(bind_status)(created_at) )
};


} //namespace amax