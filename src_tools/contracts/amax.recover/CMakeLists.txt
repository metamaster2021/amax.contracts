add_contract(amax.recover amax.recover ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.recover.cpp)

target_include_directories(amax.recover
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.recover
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.recover.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.recover.contracts.md @ONLY )

target_compile_options( amax.recover PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
