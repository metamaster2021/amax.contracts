add_contract(amax.split amax.split ${CMAKE_CURRENT_SOURCE_DIR}/src/tokensplit.cpp)

if(DEFINED ENV{DAY_SECONDS_FOR_TEST})
   message(WARNING "ENV{DAY_SECONDS_FOR_TEST}=$ENV{DAY_SECONDS_FOR_TEST} should use only for test!!!")
   target_compile_definitions(amax.split PUBLIC "DAY_SECONDS_FOR_TEST=$ENV{DAY_SECONDS_FOR_TEST}")
endif()


target_include_directories(amax.split
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(amax.split
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/tokensplit.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/tokensplit.contracts.md @ONLY )

target_compile_options( amax.split PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
