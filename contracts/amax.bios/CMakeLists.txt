add_contract(amax.bios amax.bios ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.bios.cpp)

target_include_directories(amax.bios
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.bios
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.bios.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.bios.contracts.md @ONLY )

target_compile_options( amax.bios PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
