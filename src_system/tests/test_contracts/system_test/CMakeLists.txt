add_contract(system_test system_test ${CMAKE_CURRENT_SOURCE_DIR}/src/system_test.cpp)

target_include_directories(system_test
   PUBLIC
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../../../contracts/amax.system/include)
