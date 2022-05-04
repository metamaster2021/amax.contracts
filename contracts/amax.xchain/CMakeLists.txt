add_contract(amax.xchain amax.xchain ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.xchain.cpp)

target_include_directories(amax.xchain
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.xchain
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.xchain.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.xchain.contracts.md @ONLY )

target_compile_options( amax.xchain PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
