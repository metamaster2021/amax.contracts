# return GIT_COMMIT_ID
function (get_git_commit_id)
   find_package(Git)
   if(GIT_FOUND)
      execute_process(
         COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
         OUTPUT_VARIABLE COMMIT_ID
         ERROR_VARIABLE err
         OUTPUT_STRIP_TRAILING_WHITESPACE
         RESULT_VARIABLE VERSION_COMMIT_RESULT
         WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
      )
      if(${VERSION_COMMIT_RESULT} EQUAL "0")
         execute_process(
            COMMAND ${GIT_EXECUTABLE} diff --quiet
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            RESULT_VARIABLE VERSION_DIRTY
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE )
         if(${VERSION_DIRTY})
            set(COMMIT_ID "${COMMIT_ID}-dirty")
         endif()
      else()
         message(WARNING "git rev-parse HEAD failed! ${err}")
      endif()
   endif()
   if(COMMIT_ID)
      set(PARENT_SCOPE GIT_COMMIT_ID ${COMMIT_ID})
   else()
      unset(PARENT_SCOPE GIT_COMMIT_ID)
   endif(COMMIT_ID)

endfunction()

function (get_full_version VERSION)
   get_git_commit_id()
   if(GIT_COMMIT_ID)
      set(PARENT_SCOPE FULL_VERSION "${PARENT_SCOPE}-${GIT_COMMIT_ID}")
   endif(GIT_COMMIT_ID)
endfunction()
