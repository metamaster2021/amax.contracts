add_contract(mulsign mulsign ${CMAKE_CURRENT_SOURCE_DIR}/mulsign.cpp)

target_include_directories(mulsign
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include/)

set_target_properties(mulsign
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/mulsign.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/mulsign.contracts.md @ONLY )

target_compile_options( mulsign PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
