add_contract(amax.reward amax.reward ${CMAKE_CURRENT_SOURCE_DIR}/src/amax.reward.cpp)

target_include_directories(amax.reward
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include)

set_target_properties(amax.reward
   PROPERTIES
   RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")

configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/ricardian/amax.reward.contracts.md.in ${CMAKE_CURRENT_BINARY_DIR}/ricardian/amax.reward.contracts.md @ONLY )

target_compile_options( amax.reward PUBLIC -R${CMAKE_CURRENT_SOURCE_DIR}/ricardian -R${CMAKE_CURRENT_BINARY_DIR}/ricardian )
