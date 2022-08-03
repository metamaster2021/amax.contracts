find_package(Git)
if(GIT_FOUND)
   execute_process(
      COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
      OUTPUT_VARIABLE VERSION_COMMIT_ID
      ERROR_VARIABLE err
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE VERSION_COMMIT_RESULT
      WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
   )
   if(${VERSION_COMMIT_RESULT} EQUAL "0")
      set(VERSION_FULL "${VERSION_FULL}-${VERSION_COMMIT_ID}")
      execute_process(
         COMMAND ${GIT_EXECUTABLE} diff --quiet
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
         RESULT_VARIABLE VERSION_DIRTY
         ERROR_QUIET
         OUTPUT_STRIP_TRAILING_WHITESPACE )
      if(${VERSION_DIRTY})
         set(VERSION_FULL "${VERSION_FULL}-dirty")
      endif()

   else()
      message(WARNING "git rev-parse HEAD failed! ${err}")
   endif()
endif()

configure_file( ${SRC_DIR}/ContractVersion.cmake.in ${DEST_DIR}/ContractVersion.cmake @ONLY )