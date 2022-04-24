add_contract(xtoken_deposit xtoken_deposit ${CMAKE_CURRENT_SOURCE_DIR}/src/xtoken_deposit.cpp)

target_include_directories(xtoken_deposit
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../../../contracts/amax.token/include)

set_target_properties(xtoken_deposit
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

