set(contract_name "amax.token")
add_contract(amax.token amax.token ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.token.cpp)

target_include_directories(amax.token
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.token
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.token.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.token.contracts.md @ONLY )

target_compile_options( amax.token PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )

install(
   FILES
      ${CMAKE_CURRENT_BINARY_DIR}/${contract_name}.wasm
      ${CMAKE_CURRENT_BINARY_DIR}/${contract_name}.abi
   DESTINATION
      ${CMAKE_INSTALL_PREFIX}/contracts/${contract_name}
)