add_contract(amax.custody amax.custody ${CMAKE_CURRENT_SOURCE_DIR}/src/custody.cpp)

target_include_directories(amax.custody
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.custody
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/custody.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/custody.contracts.md @ONLY )

target_compile_options( amax.custody PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
