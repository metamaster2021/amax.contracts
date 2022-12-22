add_contract(multi_token_deposit multi_token_deposit ${CMAKE_CURRENT_SOURCE_DIR}/src/multi_token_deposit.cpp)

target_include_directories(multi_token_deposit
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)
