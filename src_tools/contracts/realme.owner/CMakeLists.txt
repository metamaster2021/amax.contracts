add_contract(realme.owner realme.owner ${CMAKE_CURRENT_SOURCE_DIR}/src/realme.owner.cpp)

target_include_directories(realme.owner
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(realme.owner
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/realme.owner.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/realme.owner.contracts.md @ONLY )

target_compile_options( realme.owner PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
