add_contract(amax.applybp amax.applybp ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.applybp.cpp)

target_include_directories(amax.applybp
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/../amax.bootdao/include)

set_target_properties(amax.applybp
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.applybp.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.applybp.contracts.md @ONLY )

target_compile_options( amax.applybp PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
