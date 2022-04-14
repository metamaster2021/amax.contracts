set(WASM_WASM_OLD_BEHAVIOR "Off")
find_package(wasm.cdt)

add_contract( custody custody custody.cpp )
target_include_directories( custody PUBLIC ${CMAKE_SOURCE_DIR}/../include )
target_ricardian_directory( custody ${CMAKE_SOURCE_DIR}/../ricardian )
