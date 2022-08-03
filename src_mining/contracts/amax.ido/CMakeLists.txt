add_contract(amax.ido amax.ido ${CMAKE_CURRENT_SOURCE_DIR}/src/amax_ido.cpp)

if(DEFINED ENV{DAY_SECONDS_FOR_TEST})
   message(WARNING "ENV{DAY_SECONDS_FOR_TEST}=$ENV{DAY_SECONDS_FOR_TEST} should use only for test!!!")
   target_compile_definitions(amax.ido PUBLIC "DAY_SECONDS_FOR_TEST=$ENV{DAY_SECONDS_FOR_TEST}")
endif()


target_include_directories(amax.ido
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(amax.ido
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax_ido.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax_ido.contracts.md @ONLY )

target_compile_options( amax.ido PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
