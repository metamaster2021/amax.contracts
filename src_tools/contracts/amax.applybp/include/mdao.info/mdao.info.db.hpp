
#pragma once

#include <eosio/asset.hpp>
#include <eosio/singleton.hpp>
#include <eosio/privileged.hpp>
#include <eosio/name.hpp>
#include <map>
#include <set>

using namespace eosio;
using namespace amax;

// #define HASH256(str) sha256(const_cast<char*>(str.c_str()), str.size());
static constexpr uint64_t   seconds_per_day       = 24 * 3600;
static constexpr string_view     aplink_limit          = "aplink";
static constexpr string_view     armonia_limit         = "armonia";
static constexpr string_view     amax_limit            = "amax";
static constexpr string_view     meta_limit            = "meta";

namespace mdao {

struct dao_info_t {
    name                        dao_code;
    

    dao_info_t() {}
    dao_info_t(const name& c): dao_code(c) {}

    uint64_t    primary_key()const { return dao_code.value; }
    uint64_t    scope() const { return 0; }

    typedef eosio::multi_index
    <"infos"_n, dao_info_t
    > idx_t;

    EOSLIB_SERIALIZE( dao_info_t, (dao_code))

};

} //mdao