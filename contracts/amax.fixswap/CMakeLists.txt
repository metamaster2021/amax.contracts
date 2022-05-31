add_contract(amax.fixswap amax.fixswap ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.fixswap.cpp)

target_include_directories(amax.fixswap
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.fixswap
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.fixswap.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.fixswap.contracts.md @ONLY )

target_compile_options( amax.fixswap PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
