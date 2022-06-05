add_contract(amax.mtoken amax.mtoken ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.mtoken.cpp)

target_include_directories(amax.mtoken
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.mtoken
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.mtoken.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.mtoken.contracts.md @ONLY )

target_compile_options( amax.mtoken PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
