add_contract(realme.auth realme.auth ${CMAKE_CURRENT_SOURCE_DIR}/src/realme.auth.cpp)

target_include_directories(realme.auth
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../amax.bootdao/include)

set_target_properties(realme.auth
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/realme.auth.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/realme.auth.contracts.md @ONLY )

target_compile_options( realme.auth PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
