# CreateClangTidyTargets.cmake
#
# Adds clang-tidy-fix target for applying fixes.
# Note: clang-tidy-check is no longer needed - use CMAKE_CXX_CLANG_TIDY instead.

include_guard()

find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy-20 clang-tidy-19 clang-tidy)

function(create_clang_tidy_targets)
  cmake_language(DEFER DIRECTORY "${PROJECT_SOURCE_DIR}" CALL _create_clang_tidy_targets_impl)
endfunction()

function(_create_clang_tidy_targets_impl)
  if(NOT CLANG_TIDY_EXECUTABLE)
    message(STATUS "clang-tidy not found, skipping clang-tidy targets")
    return()
  endif()

  message(STATUS "Clang-tidy available: ${CLANG_TIDY_EXECUTABLE}")
  message(STATUS "Use -DCMAKE_CXX_CLANG_TIDY=clang-tidy to enable automatic checks during build")
endfunction()
