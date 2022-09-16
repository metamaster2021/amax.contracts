#pragma once

// #include <eosio/eosio.hpp>
// #include <eosio/asset.hpp>
#include <eosio/privileged.hpp>
#include <map>

// using namespace eosio;
// using namespace std;

namespace eosio {
   enum class producer_change_operation: uint8_t {
      add       = 0,
      modify    = 1,
      del       = 2,
   };

   enum class producer_change_format: uint64_t {
      incremental = 2
   };

   #define STR_REF(s) #s

   #define producer_authority_change(operation) \
      struct producer_authority_##operation { \
         static constexpr producer_change_operation change_operation = producer_change_operation::operation; \
         std::optional<block_signing_authority> authority; \
         EOSLIB_SERIALIZE( producer_authority_##operation, (authority) ) \
      };

   producer_authority_change(add)
   producer_authority_change(modify)
   producer_authority_change(del)

   using producer_change_record = std::variant<
      producer_authority_add,     // add
      producer_authority_modify,  // modify
      producer_authority_del      // delete
   >;

   struct producer_change_map {
      bool clear_existed = false; // clear existed producers before change
      uint32_t  producer_count = 0; // the total producer count after change
      std::map<name, producer_change_record> changes;

      EOSLIB_SERIALIZE( producer_change_map, (clear_existed)(producer_count)(changes) )
   };

   struct proposed_producer_changes {
      producer_change_map main_changes;
      producer_change_map backup_changes;
   };

   inline int64_t set_proposed_producers_ex( const proposed_producer_changes& changes ) {
      auto packed_changes = eosio::pack( changes );
      return internal_use_do_not_use::set_proposed_producers_ex((uint64_t)producer_change_format::incremental,
         (char*)packed_changes.data(), packed_changes.size());
   }

   inline std::optional<uint64_t> set_proposed_producers( const proposed_producer_changes& changes ) {
      int64_t ret = set_proposed_producers_ex(changes);
      if (ret >= 0)
        return static_cast<uint64_t>(ret);
      return {};
   }
}

