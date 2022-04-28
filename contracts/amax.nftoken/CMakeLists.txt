add_contract(amax.nftoken amax.nftoken ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.nftoken.cpp)

target_include_directories(amax.nftoken
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.nftoken
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.nftoken.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.nftoken.contracts.md @ONLY )

target_compile_options( amax.nftoken PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
