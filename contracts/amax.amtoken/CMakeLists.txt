add_contract(amax.amtoken amax.amtoken ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.amtoken.cpp)

target_include_directories(amax.amtoken
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.amtoken
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.amtoken.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.amtoken.contracts.md @ONLY )

target_compile_options( amax.amtoken PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
