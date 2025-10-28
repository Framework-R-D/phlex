# CreateCoverageTargets.cmake
#
# Adds code coverage targets for the project.
#
# This module provides the following targets when ENABLE_COVERAGE is ON:
#
# * coverage-xml:    Generates a Cobertura XML report for CI/Codecov.
# * coverage-html:   Generates a detailed HTML report for local inspection.
# * coverage-summary: Prints a human-readable summary to the console.
# * coverage-clean:  Removes all generated coverage data.
#
# It relies on the common target registration mechanism to discover which
# binaries to analyze. Targets should be registered by calling
# `phlex_add_target_for_tool(COVERAGE <target_name>)`.

# ----------------------------------------------------------------------------
# Preamble
# ----------------------------------------------------------------------------

include_guard()

# Include the common target registration module
include(Modules/private/TargetRegistration.cmake)

# ----------------------------------------------------------------------------
# Public entry point
# ----------------------------------------------------------------------------
function(phlex_create_coverage_targets)
  if(NOT ENABLE_COVERAGE)
    return()
  endif()

  # Defer the actual creation of targets until the end of the configuration,
  # so that all targets have had a chance to be registered.
  cmake_language(
    DEFER DIRECTORY "${PROJECT_SOURCE_DIR}" CALL
    _phlex_create_coverage_targets_impl
    )
endfunction()

function(_phlex_create_coverage_targets_impl)
  # Find coverage report generation tools
  find_program(LCOV_EXECUTABLE lcov)
  find_program(GENHTML_EXECUTABLE genhtml)
  find_program(GCOVR_EXECUTABLE gcovr)

  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # For clang, we need llvm-profdata and llvm-cov
    find_program(LLVM_PROFDATA_EXECUTABLE llvm-profdata)
    find_program(LLVM_COV_EXECUTABLE llvm-cov)
    if(NOT LLVM_PROFDATA_EXECUTABLE OR NOT LLVM_COV_EXECUTABLE)
      message(FATAL_ERROR "llvm-profdata and llvm-cov are required for clang coverage")
    endif()
  endif()

  # Get the list of all targets that were registered for coverage analysis.
  get_property(COVERAGE_TARGETS GLOBAL PROPERTY PHLEX_TOOL_TARGETS_COVERAGE)

  if(NOT COVERAGE_TARGETS)
    message(WARNING "Coverage is enabled, but no targets were registered for coverage analysis. The coverage reports will be empty.")
  endif()

  # Build the list of -object <binary> arguments for llvm-cov
  set(LLVM_COV_OBJECT_ARGS)
  foreach(TARGET IN LISTS COVERAGE_TARGETS)
    list(APPEND LLVM_COV_OBJECT_ARGS -object $<TARGET_FILE:${TARGET}>)
  endforeach()


  # Simple coverage target that just generates XML and HTML reports
  add_custom_target(
    coverage
    COMMAND echo "Generating coverage reports from existing coverage data..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT
      "Generating coverage reports (use coverage-xml or coverage-html for specific formats)"
    )

  # HTML coverage report using lcov (if available)
  if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
    # Use the same root detection as for XML coverage
    if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
      set(COVERAGE_SOURCE_ROOT ${PROJECT_SOURCE_DIR})
    else()
      set(COVERAGE_SOURCE_ROOT ${CMAKE_SOURCE_DIR})
    endif()

    set(LCOV_REMOVE_PATTERNS "/usr/*" "${CMAKE_BINARY_DIR}/_deps/*"
                             "*/spack*/*" "*/test/*" "*/boost/*" "*/tbb/*"
        )

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

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      add_custom_target(
        coverage-html
        COMMAND
          sh -c "find . -name '*.profraw' | xargs ${LLVM_PROFDATA_EXECUTABLE} merge -sparse -o coverage.profdata"
        COMMAND
          ${LLVM_COV_EXECUTABLE} show -format=html -output-dir=coverage-html
          -instr-profile=coverage.profdata
          -show-line-counts-or-regions -show-instantiations
          -path-equivalence=/,${CMAKE_SOURCE_DIR}
          ${LLVM_COV_OBJECT_ARGS}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating HTML coverage report with llvm-cov"
        VERBATIM
        )
    else()
      add_custom_target(
        coverage-html
        COMMAND
          ${LCOV_EXECUTABLE} --directory . --capture --output-file coverage.info
          --rc branch_coverage=1 --ignore-errors mismatch,inconsistent,negative
          --ignore-errors deprecated
        COMMAND
          ${LCOV_EXECUTABLE} --remove coverage.info ${LCOV_REMOVE_PATTERNS}
          --output-file coverage.info.cleaned --rc branch_coverage=1
          --ignore-errors mismatch,inconsistent,negative,unused,empty
        COMMAND
          ${LCOV_EXECUTABLE} --extract coverage.info.cleaned
          ${LCOV_EXTRACT_PATTERNS} --output-file coverage.info.final --rc
          branch_coverage=1 --ignore-errors
          mismatch,inconsistent,negative,empty,unused
        COMMAND
          ${GENHTML_EXECUTABLE} -o coverage-html coverage.info.final --title
          "Phlex Coverage Report" --show-details --legend --branch-coverage
          --ignore-errors mismatch,inconsistent,negative,empty
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT
          "Generating HTML coverage report with lcov (filters: ${LCOV_EXTRACT_DESCRIPTION})"
        VERBATIM
        )
    endif()
    message(
      STATUS
        "Added 'coverage-html' target using lcov with filters: ${LCOV_EXTRACT_DESCRIPTION}"
      )
  endif()

  # XML coverage report using gcovr (if available)
  if(GCOVR_EXECUTABLE)
    # Determine the appropriate source root for coverage analysis In
    # standalone mode: CMAKE_SOURCE_DIR == PROJECT_SOURCE_DIR In multi-project
    # mode: CMAKE_SOURCE_DIR is parent of PROJECT_SOURCE_DIR
    if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
      # Standalone mode - use PROJECT_SOURCE_DIR
      set(COVERAGE_SOURCE_ROOT ${PROJECT_SOURCE_DIR})
    else()
      # Multi-project mode - use CMAKE_SOURCE_DIR to include all projects but
      # filter to only include this project's files
      set(COVERAGE_SOURCE_ROOT ${CMAKE_SOURCE_DIR})
    endif()

    # Build filter arguments so gcovr only considers Phlex sources, regardless
    # of whether the workspace root is the repository root or a multi-project
    # workspace.
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
      string(REGEX REPLACE "/$" "" _gcovr_filter_trimmed
                           "${_gcovr_filter_path}"
             )
      string(REGEX REPLACE "([][.^$+*?()|\\])" "\\\\\\1"
                           _gcovr_filter_escaped "${_gcovr_filter_trimmed}"
             )
      list(APPEND GCOVR_FILTER_ARGS --filter "${_gcovr_filter_escaped}/.*")
    endforeach()

    # Exclude external dependencies and generated code that should not impact
    # project coverage metrics (Boost, TBB, system headers, etc.).
    set(GCOVR_EXCLUDE_ARGS
        --exclude
        ".*/test/.*"
        --exclude
        ".*/_deps/.*"
        --exclude
        ".*/external/.*"
        --exclude
        ".*/third[-_]?party/.*"
        --exclude
        ".*/boost/.*"
        --exclude
        ".*/tbb/.*"
        --exclude
        "/usr/.*"
        --exclude
        "/opt/.*"
        --exclude
        "/scratch/.*"
        )

    # Workaround for GCC bug #120319: "Unexpected number of branch outcomes
    # and line coverage for C++ programs"
    # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=120319 Modern GCC (15.2+)
    # has a regression causing negative hits in gcov output for complex C++
    # templates
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      add_custom_target(
        coverage-xml
        COMMAND
          sh -c "find . -name '*.profraw' | xargs ${LLVM_PROFDATA_EXECUTABLE} merge -sparse -o coverage.profdata"
        COMMAND
          ${LLVM_COV_EXECUTABLE} export -format=lcov -instr-profile=coverage.profdata
          ${LLVM_COV_OBJECT_ARGS} > coverage.info
        COMMAND
          ${LCOV_EXECUTABLE} --remove coverage.info ${LCOV_REMOVE_PATTERNS}
          --output-file coverage.info.cleaned --rc branch_coverage=1
          --ignore-errors mismatch,inconsistent,negative,unused,empty
        COMMAND
          ${GCOVR_EXECUTABLE} --root ${COVERAGE_SOURCE_ROOT}
          ${GCOVR_FILTER_ARGS} ${GCOVR_EXCLUDE_ARGS} --xml-pretty
          --exclude-unreachable-branches --print-summary
          -o coverage.xml coverage.info.cleaned
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT
          "Generating XML coverage report with llvm-cov and gcovr"
        VERBATIM
        )

      add_custom_target(
        coverage-summary
        COMMAND
          sh -c "find . -name '*.profraw' | xargs ${LLVM_PROFDATA_EXECUTABLE} merge -sparse -o coverage.profdata"
        COMMAND
          ${LLVM_COV_EXECUTABLE} export -format=lcov -instr-profile=coverage.profdata
          ${LLVM_COV_OBJECT_ARGS} > coverage.info
        COMMAND
          ${LCOV_EXECUTABLE} --remove coverage.info ${LCOV_REMOVE_PATTERNS}
          --output-file coverage.info.cleaned --rc branch_coverage=1
          --ignore-errors mismatch,inconsistent,negative,unused,empty
        COMMAND
          ${GCOVR_EXECUTABLE} --root ${COVERAGE_SOURCE_ROOT}
          ${GCOVR_FILTER_ARGS} ${GCOVR_EXCLUDE_ARGS}
          --exclude-unreachable-branches --print-summary
          coverage.info.cleaned
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT
          "Generating coverage summary with llvm-cov and gcovr"
        VERBATIM
        )
    else()
      add_custom_target(
        coverage-xml
        COMMAND
          ${GCOVR_EXECUTABLE} --root ${COVERAGE_SOURCE_ROOT}
          ${GCOVR_FILTER_ARGS} ${GCOVR_EXCLUDE_ARGS} --xml-pretty
          --exclude-unreachable-branches --print-summary
          --gcov-ignore-parse-errors=negative_hits.warn_once_per_file
          --gcov-ignore-errors=no_working_dir_found -o coverage.xml .
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT
          "Generating XML coverage report with gcovr (root: ${COVERAGE_SOURCE_ROOT})"
        VERBATIM
        )

      add_custom_target(
        coverage-summary
        COMMAND
          ${GCOVR_EXECUTABLE} --root ${COVERAGE_SOURCE_ROOT}
          ${GCOVR_FILTER_ARGS} ${GCOVR_EXCLUDE_ARGS}
          --exclude-unreachable-branches --print-summary
          --gcov-ignore-parse-errors=negative_hits.warn_once_per_file
          --gcov-ignore-errors=no_working_dir_found .
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT
          "Generating coverage summary with gcovr (root: ${COVERAGE_SOURCE_ROOT})"
        VERBATIM
        )
    endif()
    message(
      STATUS
        "Added 'coverage-xml' target using gcovr with root: ${COVERAGE_SOURCE_ROOT}"
      )
  endif()

  # Clean coverage data
  add_custom_target(
    coverage-clean
    COMMAND find ${CMAKE_BINARY_DIR} -name "*.gcda" -delete
    COMMAND find ${CMAKE_BINARY_DIR} -name "*.gcno" -delete
    COMMAND find ${CMAKE_BINARY_DIR} -name "*.profraw" -delete
    COMMAND find ${CMAKE_BINARY_DIR} -name "*.profdata" -delete
    COMMAND rm -f ${CMAKE_BINARY_DIR}/coverage.info*
    COMMAND rm -f ${CMAKE_BINARY_DIR}/coverage.xml
    COMMAND rm -rf ${CMAKE_BINARY_DIR}/coverage-html
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Cleaning coverage data files"
    )

  message(
    STATUS
      "Coverage targets added: coverage, coverage-xml, coverage-html, coverage-summary, coverage-clean"
    )

endfunction()
