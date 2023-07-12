set(contract_name amax.system)
add_contract( ${contract_name} ${contract_name}
   ${CMAKE_CURRENT_SOURCE_DIR}/src/${contract_name}.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/delegate_bandwidth.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/exchange_state.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/name_bidding.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/native.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/producer_pay.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/powerup.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/rex.cpp
   ${CMAKE_CURRENT_SOURCE_DIR}/src/voting.cpp
)

target_include_directories(${contract_name}
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../amax.token/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../amax.reward/include)

set_target_properties(${contract_name}
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

add_contract(rex.results rex.results ${CMAKE_CURRENT_SOURCE_DIR}/src/rex.results.cpp)
add_contract(powup.results powup.results ${CMAKE_CURRENT_SOURCE_DIR}/src/powerup.results.cpp)

target_include_directories(rex.results
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_include_directories(powup.results
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(rex.results
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/.rex")

set_target_properties(powup.results
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/.powerup")


configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/${contract_name}.clauses.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/${contract_name}.clauses.md @ONLY )
configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/${contract_name}.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/${contract_name}.contracts.md @ONLY )

target_compile_options( ${contract_name} PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )

install(
   FILES
      ${CMAKE_CURRENT_BINARY_DIR}/${contract_name}.wasm
      ${CMAKE_CURRENT_BINARY_DIR}/${contract_name}.abi
   DESTINATION
      ${CMAKE_INSTALL_PREFIX}/contracts/${contract_name}
)

install(
   FILES
      ${CMAKE_CURRENT_BINARY_DIR}/.rex/rex.results.wasm
      ${CMAKE_CURRENT_BINARY_DIR}/.rex/rex.results.abi
   DESTINATION
      ${CMAKE_INSTALL_PREFIX}/contracts/rex.results
)

install(
   FILES
      ${CMAKE_CURRENT_BINARY_DIR}/.powerup/powup.results.wasm
      ${CMAKE_CURRENT_BINARY_DIR}/.powerup/powup.results.abi
   DESTINATION
      ${CMAKE_INSTALL_PREFIX}/contracts/powup.results
)
