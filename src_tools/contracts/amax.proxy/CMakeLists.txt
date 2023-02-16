add_contract(amax.proxy amax.proxy ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.proxy.cpp)

target_include_directories(amax.proxy
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../amax.bootdao/include)

set_target_properties(amax.proxy
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.proxy.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.proxy.contracts.md @ONLY )

target_compile_options( amax.proxy PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
