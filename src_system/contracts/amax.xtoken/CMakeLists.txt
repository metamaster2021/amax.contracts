add_contract(amax.xtoken amax.xtoken ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.xtoken.cpp)

target_include_directories(amax.xtoken
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.xtoken
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.xtoken.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.xtoken.contracts.md @ONLY )

target_compile_options( amax.xtoken PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
