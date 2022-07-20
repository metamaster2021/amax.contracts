add_contract(amax.mulsign amax.mulsign ${CMAKE_CURRENT_SOURCE_DIR}/amax.mulsign.cpp)

target_include_directories(amax.mulsign
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include/)

set_target_properties(amax.mulsign
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.mulsign.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.mulsign.contracts.md @ONLY )

target_compile_options( amax.mulsign PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
