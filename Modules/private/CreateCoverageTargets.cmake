# CreateCoverageTargets.cmake
#
# Adds code coverage targets for the project:
# * coverage: Generates coverage reports from existing coverage data
# * coverage-html: Generates HTML coverage report using lcov/genhtml
# * coverage-xml: Generates XML coverage report using gcovr
# * coverage-clean: Cleans coverage data files
#
# Usage:
#   create_coverage_targets()
#
# Options:
#   ENABLE_COVERAGE must be ON
#   Requires: CMake >= 3.22

include_guard()

# Find coverage report generation tools
find_program(LCOV_EXECUTABLE lcov)
find_program(GENHTML_EXECUTABLE genhtml)
find_program(GCOVR_EXECUTABLE gcovr)

# Find CTest coverage tool
find_program(CTEST_COVERAGE_COMMAND NAMES gcov llvm-cov DOC "Coverage tool for CTest" CACHE)
if(CTEST_COVERAGE_COMMAND)
    message(STATUS "Found coverage tool for CTest: ${CTEST_COVERAGE_COMMAND}")
else()
    message(WARNING "Coverage tool for CTest not found - coverage reports may not work with CTest.")
endif()

if(NOT LCOV_EXECUTABLE)
    message(WARNING "lcov not found; HTML coverage reports will not be available.")
endif()
if(NOT GENHTML_EXECUTABLE)
    message(WARNING "genhtml not found; HTML coverage reports will not be available.")
endif()
if(NOT GCOVR_EXECUTABLE)
    message(WARNING "gcovr not found; XML coverage reports will not be available.")
endif()

function(create_coverage_targets)
  if(ENABLE_COVERAGE AND NOT BUILD_TESTING)
    message(STATUS "ENABLE_COVERAGE is set but BUILD_TESTING is not: coverage targets will be created, but no tests will be built.")
  endif()
  cmake_language(
    DEFER DIRECTORY "${PROJECT_SOURCE_DIR}" CALL
    _create_coverage_targets_impl
  )
endfunction()

function(_create_coverage_targets_impl)
  if(NOT ENABLE_COVERAGE)
    return()
  endif()

  # Prevent duplicate target creation
  get_property(_coverage_defined GLOBAL PROPERTY _PHLEX_COVERAGE_TARGETS_DEFINED)
  if(_coverage_defined)
    message(WARNING "Coverage targets already defined; skipping duplicate creation.")
    return()
  endif()
  set_property(GLOBAL PROPERTY _PHLEX_COVERAGE_TARGETS_DEFINED TRUE)

  add_custom_target(
    coverage
    COMMAND echo "Generating coverage reports from existing coverage data..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating coverage reports (use coverage-xml or coverage-html for specific formats)"
  )

  # HTML coverage report using lcov (if available)
  if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
    if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
      set(COVERAGE_SOURCE_ROOT ${PROJECT_SOURCE_DIR})
    else()
      set(COVERAGE_SOURCE_ROOT ${CMAKE_SOURCE_DIR})
    endif()

    set(LCOV_REMOVE_PATTERNS "/usr/*" "${CMAKE_BINARY_DIR}/_deps/*" "*/spack*/*" "*/test/*" "*/boost/*" "*/tbb/*")

    set(_lcov_extract_paths ${PROJECT_SOURCE_DIR})
    get_filename_component(_lcov_project_real ${PROJECT_SOURCE_DIR} REALPATH)
    if(NOT _lcov_project_real STREQUAL ${PROJECT_SOURCE_DIR})
      list(APPEND _lcov_extract_paths ${_lcov_project_real})
    endif()

    set(_lcov_build_dir ${CMAKE_BINARY_DIR}/${PROJECT_NAME})
    if(EXISTS ${_lcov_build_dir})
      list(APPEND _lcov_extract_paths ${_lcov_build_dir})
      get_filename_component(_lcov_build_real ${_lcov_build_dir} REALPATH)
      if(NOT _lcov_build_real STREQUAL ${_lcov_build_dir})
        list(APPEND _lcov_extract_paths ${_lcov_build_real})
      endif()
    endif()

    set(LCOV_EXTRACT_PATTERNS)
    foreach(_lcov_path IN LISTS _lcov_extract_paths)
      list(APPEND LCOV_EXTRACT_PATTERNS "${_lcov_path}/*")
    endforeach()
    list(REMOVE_DUPLICATES LCOV_EXTRACT_PATTERNS)
    string(JOIN ", " LCOV_EXTRACT_DESCRIPTION ${LCOV_EXTRACT_PATTERNS})

    add_custom_target(
      coverage-html
      COMMAND ${LCOV_EXECUTABLE} --directory . --capture --output-file coverage.info --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,negative --ignore-errors deprecated
      COMMAND ${LCOV_EXECUTABLE} --remove coverage.info ${LCOV_REMOVE_PATTERNS} --output-file coverage.info.cleaned --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,negative,unused,empty
      COMMAND ${LCOV_EXECUTABLE} --extract coverage.info.cleaned ${LCOV_EXTRACT_PATTERNS} --output-file coverage.info.final --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,negative,empty,unused
      COMMAND ${GENHTML_EXECUTABLE} -o coverage-html coverage.info.final --title "Phlex Coverage Report" --show-details --legend --branch-coverage --ignore-errors mismatch,inconsistent,negative,empty
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Generating HTML coverage report with lcov (filters: ${LCOV_EXTRACT_DESCRIPTION})"
      VERBATIM
    )
    
    add_dependencies(coverage coverage-html)
    message(STATUS "Added 'coverage-html' target using lcov with filters: ${LCOV_EXTRACT_DESCRIPTION}")
  endif()

  # XML coverage report using gcovr (if available)
  if(GCOVR_EXECUTABLE)
    if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
      set(COVERAGE_SOURCE_ROOT ${PROJECT_SOURCE_DIR})
    else()
      set(COVERAGE_SOURCE_ROOT ${CMAKE_SOURCE_DIR})
    endif()

    set(_gcovr_filter_paths ${PROJECT_SOURCE_DIR})
    get_filename_component(_gcovr_project_real ${PROJECT_SOURCE_DIR} REALPATH)
    if(NOT _gcovr_project_real STREQUAL ${PROJECT_SOURCE_DIR})
      list(APPEND _gcovr_filter_paths ${_gcovr_project_real})
    endif()

    set(_gcovr_binary_dir ${CMAKE_BINARY_DIR}/${PROJECT_NAME})
    if(EXISTS ${_gcovr_binary_dir})
      list(APPEND _gcovr_filter_paths ${_gcovr_binary_dir})
      get_filename_component(_gcovr_binary_real ${_gcovr_binary_dir} REALPATH)
      if(NOT _gcovr_binary_real STREQUAL ${_gcovr_binary_dir})
        list(APPEND _gcovr_filter_paths ${_gcovr_binary_real})
      endif()
    endif()

    list(REMOVE_DUPLICATES _gcovr_filter_paths)

    set(GCOVR_FILTER_ARGS)
    foreach(_gcovr_filter_path IN LISTS _gcovr_filter_paths)
      string(REGEX REPLACE "/$" "" _gcovr_filter_trimmed "${_gcovr_filter_path}")
      string(REGEX REPLACE "([][.^$+*?()|\\])" "\\\\\\1" _gcovr_filter_escaped "${_gcovr_filter_trimmed}")
      list(APPEND GCOVR_FILTER_ARGS --filter "${_gcovr_filter_escaped}/.*")
    endforeach()

    set(GCOVR_EXCLUDE_ARGS
      --exclude ".*/test/.*"
      --exclude ".*/_deps/.*"
      --exclude ".*/external/.*"
      --exclude ".*/third[-_]?party/.*"
      --exclude ".*/boost/.*"
      --exclude ".*/tbb/.*"
      --exclude "/usr/.*"
      --exclude "/opt/.*"
      --exclude "/scratch/.*"
    )

    add_custom_target(
      coverage-xml
      COMMAND ${GCOVR_EXECUTABLE} --root ${COVERAGE_SOURCE_ROOT} ${GCOVR_FILTER_ARGS} ${GCOVR_EXCLUDE_ARGS} --xml-pretty --exclude-unreachable-branches --print-summary --gcov-ignore-parse-errors=negative_hits.warn_once_per_file --gcov-ignore-errors=no_working_dir_found -o coverage.xml .
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Generating XML coverage report with gcovr (root: ${COVERAGE_SOURCE_ROOT})"
      VERBATIM
    )
    
    add_dependencies(coverage coverage-xml)
    message(STATUS "Added 'coverage-xml' target using gcovr with root: ${COVERAGE_SOURCE_ROOT}")
  endif()

  add_custom_target(
    coverage-clean
    COMMAND find ${CMAKE_BINARY_DIR} -name "*.gcda" -delete
    COMMAND find ${CMAKE_BINARY_DIR} -name "*.gcno" -delete
    COMMAND rm -f ${CMAKE_BINARY_DIR}/coverage.info*
    COMMAND rm -f ${CMAKE_BINARY_DIR}/coverage.xml
    COMMAND rm -rf ${CMAKE_BINARY_DIR}/coverage_html
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Cleaning coverage data files"
  )

  message(STATUS "Coverage targets added: coverage, coverage-xml, coverage-html, coverage-clean")
endfunction()
