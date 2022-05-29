add_contract(amax.ntoken amax.ntoken ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.ntoken.cpp)

target_include_directories(amax.ntoken
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.ntoken
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.ntoken.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.ntoken.contracts.md @ONLY )

target_compile_options( amax.ntoken PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
