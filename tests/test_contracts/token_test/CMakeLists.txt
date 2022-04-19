
add_contract( token_test token_test token_test.cpp )

target_include_directories(token_test
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/../../../contracts/amax.token/include
)