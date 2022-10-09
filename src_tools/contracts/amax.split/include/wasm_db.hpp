#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

namespace wasm { namespace db {

using namespace eosio;


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

    bool erase_by_pk(uint64_t pk) {
        auto itr = base::find(pk);
        if (itr != base::end()) {
            base::erase(itr);
            return true;
        }
        return false;
    }
};

enum return_t{
    NONE    = 0,
    MODIFIED,
    APPENDED,
};

class dbc {
private:
    name code;   //contract owner

public:   
    dbc() {}
    dbc(const name& code): code(code) {}

    template<typename RecordType>
    bool get(RecordType& record) {
        auto scope = code.value;

        typename RecordType::tbl_t idx(code, scope);
        if (idx.find(record.primary_key()) == idx.end())
            return false;

        record = idx.get(record.primary_key());
        return true;
    }
    template<typename RecordType>
    bool get(const uint64_t& scope, RecordType& record) {
        typename RecordType::tbl_t idx(code, scope);
        if (idx.find(record.primary_key()) == idx.end())
            return false;

        record = idx.get(record.primary_key());
        return true;
    }
  
    template<typename RecordType>
    auto get_idx(RecordType& record) {
        auto scope = record.scope();
        if (scope == 0) scope = code.value;

        typename RecordType::tbl_t idx(code, scope);
        return idx;
    }

    template<typename RecordType>
    return_t set(const RecordType& record, const name& payer) {
        auto scope = code.value;

        typename RecordType::tbl_t idx(code, scope);
        auto itr = idx.find( record.primary_key() );
        if ( itr != idx.end()) {
            idx.modify( itr, same_payer, [&]( auto& item ) {
                item = record;
            });
            return return_t::MODIFIED;

        } else {
            idx.emplace( payer, [&]( auto& item ) {
                item = record;
            });
            return return_t::APPENDED;
        }
    }

    template<typename RecordType>
    return_t set(const RecordType& record) {
       return set(record, code);
    }

    template<typename RecordType>
    return_t set(const uint64_t& scope, const RecordType& record, const bool& isModify = true) {
        typename RecordType::tbl_t idx(code, scope);
        
        if (isModify) {
            auto itr = idx.find( record.primary_key() );
            check( itr != idx.end(), "record not found" );
            idx.modify( itr, code, [&]( auto& item ) {
                item = record;
            });
            return return_t::MODIFIED;
        } 

        idx.emplace( code, [&]( auto& item ) {
            item = record;
        });
        return return_t::APPENDED;
    }

    template<typename RecordType>
    void del(const RecordType& record) {
        auto scope = code.value;

        typename RecordType::tbl_t idx(code, scope);
        auto itr = idx.find(record.primary_key());
        if ( itr != idx.end() ) {
            idx.erase(itr);
        }
    }

    template<typename RecordType>
    void del(const uint64_t& scope, const RecordType& record) {
        typename RecordType::tbl_t idx(code, scope);
        auto itr = idx.find(record.primary_key());
        if ( itr != idx.end() ) {
            idx.erase(itr);
        }
    }
};

}}//db//wasm