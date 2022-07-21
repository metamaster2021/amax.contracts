#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

namespace wasm { namespace db {

template<eosio::name::raw TableName, typename T, typename... Indices>
class multi_index_ex: public eosio::multi_index<TableName, T, Indices...> {
public:
    using base = eosio::multi_index<TableName, T, Indices...>;
    using base::base;

    template<typename Lambda>
    void set(uint64_t pk, eosio::name payer, Lambda&& setter ) {
        auto itr = base::find(pk);
        if (itr == base::end()) {
            base::emplace(payer, setter);
        } else {
            base::modify(itr, payer, setter);
        }
    }
};

}}//db//wasm