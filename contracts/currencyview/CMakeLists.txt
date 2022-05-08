add_contract(currencyview currencyview ${CMAKE_CURRENT_SOURCE_DIR}/currencyview.cpp)

target_include_directories(currencyview
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/)

set_target_properties(currencyview
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/currencyview.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/currencyview.contracts.md @ONLY )

target_compile_options( currencyview PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
