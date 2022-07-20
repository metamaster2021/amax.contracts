add_contract(amax.wrap amax.wrap ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.wrap.cpp)

target_include_directories(amax.wrap
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.wrap
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.wrap.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.wrap.contracts.md @ONLY )

target_compile_options( amax.wrap PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
