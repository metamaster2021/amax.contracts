add_contract(amax.msig amax.msig ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.msig.cpp)

target_include_directories(amax.msig
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.msig
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.msig.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.msig.contracts.md @ONLY )

target_compile_options( amax.msig PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
