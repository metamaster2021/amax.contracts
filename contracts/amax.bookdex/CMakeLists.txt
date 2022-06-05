add_contract(amax.bookdex amax.bookdex ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.bookdex.cpp)

target_include_directories(amax.bookdex
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.bookdex
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.bookdex.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.bookdex.contracts.md @ONLY )

target_compile_options( amax.bookdex PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
