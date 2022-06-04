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

#include "utils.hpp"

namespace amax {

using namespace std;
using namespace eosio;

#define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size())
#define TBL struct [[eosio::table, eosio::contract("amax.bookdex")]]

// price for ARC20 tokens
struct price_s {
    string base_symb;
    string quote_symb;
    float amount;                //base = amount * quote

    price_s() {}
    price_s(const string& bs, const string& qs, const uint64_t& am): base_symb(bs), quote_symb(qs), amount(am) {}

    static name sym_pair(const string& sym1, const string& sym2) {
        auto sym_pair = str_tolower(sym1 + sym2);
        return name(sym_pair);
    }
    name sym_pair()const { 
        auto sym_pair = base_symb + quote_symb;
        sym_pair = str_tolower(sym_pair);

        return name(sym_pair);
    }

    EOSLIB_SERIALIZE( price_s, (base_symb)(quote_symb)(amount) )
};

//only support unquie of pair of base & quote symbol, regardless of their issuance token contract
TBL trade_pair_t {
    extended_symbol base_symb;      //E.g. USDT
    extended_symbol quote_symb;     //E.g. CNYD
    float           current_price;

    trade_pair_t() {}

    uint64_t primary_key()const { return price_s::sym_pair(base_symb.get_symbol().code().to_string(), quote_symb.get_symbol().code().to_string()).value; }

    typedef eosio::multi_index< "tradepairs"_n,  trade_pair_t> idx_t;

    EOSLIB_SERIALIZE( trade_pair_t, (base_symb)(quote_symb)(current_price) )
};

//scope sym_pair
TBL offer_t {
    uint64_t    id;                    //PK
    price_s     price;
    int64_t     amount;               //buy: quote amount; sell: base amount
    name        maker;                //order maker
    time_point  created_at;
    time_point  updated_at;

    offer_t() {}
    offer_t(const uint64_t& i):id(i) {}

    uint64_t primary_key()const { return id; }
    uint64_t by_small_price_first()const { return price.amount; }
    uint64_t by_large_price_first()const { return( std::numeric_limits<uint64_t>::max() - price.amount ); }

    EOSLIB_SERIALIZE( offer_t, (id)(price)(amount)(maker)(created_at)(updated_at) )
};

//below is meant for buyers to match with
typedef eosio::multi_index
< "baseoffers"_n,  offer_t,
        indexed_by<"priceidx"_n,  const_mem_fun<offer_t, uint64_t, &offer_t::by_small_price_first> >
> baseoffer_idx;

//below is meant for sellers to match with
typedef eosio::multi_index
< "quoteoffers"_n,  offer_t,
        indexed_by<"priceidx"_n,  const_mem_fun<offer_t, uint64_t, &offer_t::by_large_price_first> >
> quoteoffer_idx;

} //namespace amax