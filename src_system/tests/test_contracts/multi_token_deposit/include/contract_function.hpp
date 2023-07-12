#pragma once

#include <eosio/eosio.hpp>


namespace amax {
   /**
    * Unpack the received action and execute the correponding action handler
    *
    * @ingroup dispatcher
    * @tparam T - The contract class that has the correponding action handler, this contract should be derived from eosio::contract
    * @tparam Args - The arguments that the action handler accepts, i.e. members of the action
    * @param contract - The contract object that has the correponding action handler
    * @param func - The action handler
    * @return true
    */
   template<typename T, typename... Args>
   void execute_contract_function( T* contract, void (T::*func)(Args...)  ) {
      size_t size = eosio::action_data_size();

      //using malloc/free here potentially is not exception-safe, although WASM doesn't support exceptions
      constexpr size_t max_stack_buffer_size = 512;
      void* buffer = nullptr;
      if( size > 0 ) {
         buffer = max_stack_buffer_size < size ? malloc(size) : alloca(size);
         eosio::read_action_data( buffer, size );
      }

      std::tuple<std::decay_t<Args>...> args;
      eosio::datastream<const char*> ds((char*)buffer, size);
      ds >> args;

      auto f2 = [&]( auto... a ){
         (contract->*func)( a... );
      };

      boost::mp11::tuple_apply( f2, args );

      if ( max_stack_buffer_size < size ) {
         free(buffer);
      }
   }

    #define execute_function(func) execute_contract_function(this, func)
}