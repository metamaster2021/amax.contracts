add_contract(amax.test amax.test ${CMAKE_CURRENT_SOURCE_DIR}/src/testcontract.cpp)

target_include_directories(amax.test
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(amax.test
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/testcontract.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/testcontract.contracts.md @ONLY )

target_compile_options( amax.test PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
