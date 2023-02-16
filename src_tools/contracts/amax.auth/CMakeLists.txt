add_contract(amax.auth amax.auth ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.auth.cpp)

target_include_directories(amax.auth
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../amax.bootdao/include)

set_target_properties(amax.auth
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.auth.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.auth.contracts.md @ONLY )

target_compile_options( amax.auth PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
