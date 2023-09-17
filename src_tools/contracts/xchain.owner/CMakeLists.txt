add_contract(xchain.owner xchain.owner ${CMAKE_CURRENT_SOURCE_DIR}/src/xchain.owner.cpp)

target_include_directories(xchain.owner
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(xchain.owner
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/xchain.owner.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/xchain.owner.contracts.md @ONLY )

target_compile_options( xchain.owner PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
