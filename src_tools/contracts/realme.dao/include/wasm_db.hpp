#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>

namespace wasm { namespace db {

using namespace eosio;

enum return_t{
    NONE    = 0,
    MODIFIED,
    APPENDED,
};

class dbc {
private:
    name code;   //contract owner

public:
    dbc(const name& code): code(code) {}

    template<typename RecordType>
    bool get(RecordType& record) {
        auto scope = code.value;

        typename RecordType::idx_t idx(code, scope);
        if (idx.find(record.primary_key()) == idx.end())
            return false;

        record = idx.get(record.primary_key());
        return true;
    }
    template<typename RecordType>
    bool get(const uint64_t& scope, RecordType& record) {
        typename RecordType::idx_t idx(code, scope);
        if (idx.find(record.primary_key()) == idx.end())
            return false;

        record = idx.get(record.primary_key());
        return true;
    }
  
    template<typename RecordType>
    auto get_idx(RecordType& record) {
        auto scope = record.scope();
        if (scope == 0) scope = code.value;

        typename RecordType::idx_t idx(code, scope);
        return idx;
    }

    template<typename RecordType>
    return_t set(const RecordType& record, const name& payer) {
        auto scope = code.value;

        typename RecordType::idx_t idx(code, scope);
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
        auto scope = code.value;

        typename RecordType::idx_t idx(code, scope);
        auto itr = idx.find( record.primary_key() );
        check( itr != idx.end(), "amax.reocver: record not found" );

        idx.modify( itr, same_payer, [&]( auto& item ) {
            item = record;
        });
        return return_t::MODIFIED;
    }

    template<typename RecordType>
    return_t set(const uint64_t& scope, const RecordType& record, const bool& isModify = true) {
        typename RecordType::idx_t idx(code, scope);
        
        if (isModify) {
            auto itr = idx.find( record.primary_key() );
            check( itr != idx.end(),  +  "amax.reocver: record not found" );
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

        typename RecordType::idx_t idx(code, scope);
        auto itr = idx.find(record.primary_key());
        if ( itr != idx.end() ) {
            idx.erase(itr);
        }
    }

    template<typename RecordType>
    void del_scope(const uint64_t& scope, const RecordType& record) {
        typename RecordType::idx_t idx(code, scope);
        auto itr = idx.find(record.primary_key());
        if ( itr != idx.end() ) {
            idx.erase(itr);
        }
    }

};

}}//db//wasm