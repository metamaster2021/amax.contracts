add_contract(amax.two amax.two ${CMAKE_CURRENT_SOURCE_DIR}/src/amax_two.cpp)

# if(DEFINED ENV{DAY_SECONDS_FOR_TEST})
#    message(WARNING "ENV{DAY_SECONDS_FOR_TEST}=$ENV{DAY_SECONDS_FOR_TEST} should use only for test!!!")
#    target_compile_definitions(amax.custody PUBLIC "DAY_SECONDS_FOR_TEST=$ENV{DAY_SECONDS_FOR_TEST}")
# endif()


target_include_directories(amax.two
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(amax.two
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

# configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax_two.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax_two.contracts.md @ONLY )

target_compile_options( amax.two PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )


