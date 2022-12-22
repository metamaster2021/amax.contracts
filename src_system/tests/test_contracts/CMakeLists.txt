
set(AMAX_WASM_OLD_BEHAVIOR "Off")
find_package( amax.cdt REQUIRED )


if ("${CMAKE_GENERATOR}" STREQUAL "Ninja")
   add_compile_options(-fcolor-diagnostics)
endif()

add_subdirectory( token_test )
add_subdirectory( xtoken_deposit )
add_subdirectory( multi_token_deposit )
add_subdirectory( system_test )
