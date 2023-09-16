add_contract(realme.dao realme.dao ${CMAKE_CURRENT_SOURCE_DIR}/src/realme.dao.cpp)

target_include_directories(realme.dao
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../amax.bootdao/include)

set_target_properties(realme.dao
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/realme.dao.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/realme.dao.contracts.md @ONLY )

target_compile_options( realme.dao PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
