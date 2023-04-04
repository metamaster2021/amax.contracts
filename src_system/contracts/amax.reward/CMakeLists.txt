set(contract_name amax.reward)
add_contract(${contract_name} ${contract_name} ${CMAKE_CURRENT_SOURCE_DIR}/src/${contract_name}.cpp)

target_include_directories(${contract_name}
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(${contract_name}
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

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