# CreateCoverageTargets.cmake
#
# Adds code coverage targets for the project:
#
# * coverage: Generates coverage reports from existing coverage data
# * coverage-html: Generates HTML coverage report using lcov/genhtml
# * coverage-xml: Generates XML coverage report using gcovr
# * coverage-clean: Cleans coverage data files
#
# ~~~
# Usage:
#   create_coverage_targets()
#
# Options:
#   ENABLE_COVERAGE must be ON
#   Requires: CMake >= 3.22
# ~~~

include_guard()

include(${CMAKE_CURRENT_LIST_DIR}/PhlexTargetUtils.cmake)

# Find coverage report generation tools
find_program(LCOV_EXECUTABLE lcov)
find_program(GENHTML_EXECUTABLE genhtml)
find_program(GCOVR_EXECUTABLE gcovr)

# Find Python3 and normalization scripts
find_package(Python3 COMPONENTS Interpreter)

# Find CTest coverage tool
find_program(
  LLVM_COV_EXECUTABLE
  NAMES llvm-cov-21 llvm-cov
  DOC "LLVM coverage tool"
  )
find_program(
  LLVM_PROFDATA_EXECUTABLE
  NAMES llvm-profdata-21 llvm-profdata
  DOC "LLVM profdata tool"
  )
if(NOT LCOV_EXECUTABLE)
  message(
    WARNING "lcov not found; HTML coverage reports will not be available."
    )
endif()
if(NOT GENHTML_EXECUTABLE)
  message(
    WARNING "genhtml not found; HTML coverage reports will not be available."
    )
endif()
if(NOT GCOVR_EXECUTABLE)
  message(
    WARNING "gcovr not found; XML coverage reports will not be available."
    )
endif()

function(create_coverage_targets)
  if(ENABLE_COVERAGE AND NOT BUILD_TESTING)
    message(
      STATUS
        "ENABLE_COVERAGE is set but BUILD_TESTING is not: coverage targets will be created, but no tests will be built."
      )
  endif()
  cmake_language(
    DEFER DIRECTORY "${PROJECT_SOURCE_DIR}" CALL _create_coverage_targets_impl
    )
endfunction()

function(_create_coverage_targets_impl)
  if(NOT ENABLE_COVERAGE)
    return()
  endif()

  # Prevent duplicate target creation
  get_property(
    _coverage_defined GLOBAL PROPERTY _PHLEX_COVERAGE_TARGETS_DEFINED
    )
  if(_coverage_defined)
    message(
      WARNING "Coverage targets already defined; skipping duplicate creation."
      )
    return()
  endif()
  set_property(GLOBAL PROPERTY _PHLEX_COVERAGE_TARGETS_DEFINED TRUE)

  # Shared gcovr exclusion arguments for all coverage targets
  set(GCOVR_EXCLUDE_ARGS
      # cmake-format: off
      --exclude ".*/test/.*"
      --exclude ".*/_deps/.*"
      --exclude ".*/external/.*"
      --exclude ".*/third[-_]?party/.*"
      --exclude ".*/boost/.*"
      --exclude ".*/tbb/.*"
  --exclude ".*/spack/.*"
      --exclude "/usr/.*"
      --exclude "/opt/.*"
      --exclude "/scratch/.*"
      --exclude [=[.*\.cxx$]=]
      --exclude [=[.*\.hh$]=]
      --exclude [=[.*\.hxx$]=]
      # cmake-format: on
      )
  # Clang/llvm-cov coverage target
  if(LLVM_COV_EXECUTABLE AND LLVM_PROFDATA_EXECUTABLE)
    set(PROFRAW_LIST_FILE ${CMAKE_BINARY_DIR}/profraw_list.txt)
    set(LLVM_PROFDATA_OUTPUT ${CMAKE_BINARY_DIR}/coverage.profdata)
    set(LLVM_COV_OUTPUT ${CMAKE_BINARY_DIR}/coverage-llvm.txt)
    set(LLVM_COV_LCOV_OUTPUT ${CMAKE_BINARY_DIR}/coverage-llvm.info)

    # Collect all executables and libraries for coverage
    phlex_collect_targets_by_type(PHLEX_EXECUTABLES "EXECUTABLE")
    phlex_collect_targets_by_type(PHLEX_LIBRARIES "STATIC_LIBRARY")
    phlex_collect_targets_by_type(PHLEX_SHARED_LIBRARIES "SHARED_LIBRARY")

    set(LLVM_COV_OBJECTS)
    foreach(_exe IN LISTS PHLEX_EXECUTABLES)
      list(APPEND LLVM_COV_OBJECTS "-object" "$<TARGET_FILE:${_exe}>")
    endforeach()
    foreach(_lib IN LISTS PHLEX_LIBRARIES PHLEX_SHARED_LIBRARIES)
      list(APPEND LLVM_COV_OBJECTS "-object" "$<TARGET_FILE:${_lib}>")
    endforeach()

    # Exclusion regex for llvm-cov (same as gcovr)
    set(LLVM_COV_EXCLUDE_REGEX
        [=[.*/test/.*|.*/_deps/.*|.*/external/.*|.*/third[-_]?party/.*|.*/boost/.*|.*/tbb/.*|.*/spack/.*|/usr/.*|/opt/.*|/scratch/.*|.*\.cxx$|.*\.hh$|.*\.hxx$]=]
        )

    # 1. Merge all valid .profraw files into coverage.profdata
    add_custom_command(
      OUTPUT ${LLVM_PROFDATA_OUTPUT}
      COMMAND ${CMAKE_COMMAND} -E echo
              "[Coverage] Scanning for profile data files (*.profraw)"
      COMMAND ${CMAKE_COMMAND} -E rm -f ${PROFRAW_LIST_FILE}
      COMMAND
        bash -c
        "find test -name '*.profraw' -type f -size +0c > ${PROFRAW_LIST_FILE}"
      COMMAND
        ${CMAKE_COMMAND} -E echo
        "[Coverage] Merging profile data files listed in ${PROFRAW_LIST_FILE}"
      COMMAND
        bash -c
        "set -euo pipefail; mapfile -t profs < ${PROFRAW_LIST_FILE}; \
                 if [ \${#profs[@]} -eq 0 ]; then \
                   echo 'No profile data files were generated. Ensure tests were run with coverage instrumentation.' >&2; \
                   exit 1; \
                 fi; \
                 \"${LLVM_PROFDATA_EXECUTABLE}\" merge --failure-mode=warn -sparse \"\${profs[@]}\" -o \"${LLVM_PROFDATA_OUTPUT}\""
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      BYPRODUCTS ${PROFRAW_LIST_FILE}
      COMMENT "Collecting and merging coverage profile data files"
      VERBATIM
      )

    # 1. Produce the coverage report
    add_custom_command(
      OUTPUT ${LLVM_COV_OUTPUT}
      DEPENDS ${LLVM_PROFDATA_OUTPUT}
      COMMAND ${CMAKE_COMMAND} -E echo
              "[Coverage] Generating coverage report using llvm-cov"
      COMMAND
        ${LLVM_COV_EXECUTABLE} report ${LLVM_COV_OBJECTS}
        -instr-profile=${LLVM_PROFDATA_OUTPUT}
        "-ignore-filename-regex=${LLVM_COV_EXCLUDE_REGEX}" > ${LLVM_COV_OUTPUT}
      COMMENT "Generating coverage report with llvm-cov"
      VERBATIM COMMAND_EXPAND_LISTS
      )

    set(LLVM_COV_OBJECTS_QUOTED)
    foreach(_cov_arg IN LISTS LLVM_COV_OBJECTS)
      string(REPLACE "\"" "\\\"" _cov_arg_escaped "${_cov_arg}")
      list(APPEND LLVM_COV_OBJECTS_QUOTED "\"${_cov_arg_escaped}\"")
    endforeach()
    list(JOIN LLVM_COV_OBJECTS_QUOTED " " LLVM_COV_OBJECTS_SHELL)

    set(_llvm_cov_export_command
        "set -euo pipefail; \"${LLVM_COV_EXECUTABLE}\" export ${LLVM_COV_OBJECTS_SHELL} -instr-profile=\"${LLVM_PROFDATA_OUTPUT}\" \"-ignore-filename-regex=${LLVM_COV_EXCLUDE_REGEX}\" --format=lcov > \"${LLVM_COV_LCOV_OUTPUT}\""
        )

    add_custom_command(
      OUTPUT ${LLVM_COV_LCOV_OUTPUT}
      DEPENDS ${LLVM_PROFDATA_OUTPUT}
      COMMAND
        ${CMAKE_COMMAND} -E echo
        "[Coverage] Exporting LLVM coverage data to LCOV (${LLVM_COV_LCOV_OUTPUT})"
      COMMAND bash -c "${_llvm_cov_export_command}"
      COMMENT "Exporting LLVM coverage data to LCOV"
      VERBATIM COMMAND_EXPAND_LISTS
      )

    add_custom_target(coverage-llvm-lcov DEPENDS ${LLVM_COV_LCOV_OUTPUT})

    # Normalization target for llvm-cov output (if Python script exists)
    set(_normalize_llvm_script
        "${PROJECT_SOURCE_DIR}/scripts/normalize_coverage_lcov.py"
        )
    set(LLVM_COV_NORMALIZED_STAMP
        ${CMAKE_BINARY_DIR}/coverage-llvm-normalized.stamp
        )
    if(Python3_FOUND AND EXISTS "${_normalize_llvm_script}")
      add_custom_command(
        OUTPUT ${LLVM_COV_NORMALIZED_STAMP}
        DEPENDS ${LLVM_COV_OUTPUT}
        COMMAND
          ${Python3_EXECUTABLE} "${_normalize_llvm_script}" --repo-root
          "${PROJECT_SOURCE_DIR}" --coverage-root "${PROJECT_SOURCE_DIR}"
          --coverage-alias "${PROJECT_SOURCE_DIR}" "${LLVM_COV_OUTPUT}"
        COMMAND ${CMAKE_COMMAND} -E touch ${LLVM_COV_NORMALIZED_STAMP}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Normalizing LLVM coverage report for editor/CI tooling"
        VERBATIM
        )
      add_custom_target(
        coverage-llvm-normalize DEPENDS ${LLVM_COV_NORMALIZED_STAMP}
        )
      set(_coverage_llvm_primary_dependency coverage-llvm-normalize)
    else()
      add_custom_target(
        coverage-llvm-normalize
        COMMAND
          ${CMAKE_COMMAND} -E echo
          "ERROR: Python3 or normalize_coverage_lcov.py not found. Cannot normalize LLVM coverage report."
        COMMAND exit 1
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Failed to normalize LLVM coverage report"
        )
      set(_coverage_llvm_primary_dependency ${LLVM_COV_OUTPUT})
    endif()

    # Orchestrate generation, normalization, and summary presentation
    add_custom_target(
      coverage-llvm
      DEPENDS ${_coverage_llvm_primary_dependency} coverage-llvm-lcov
      COMMAND ${CMAKE_COMMAND} -E echo "[Coverage] LLVM coverage summary:"
      COMMAND
        bash -c
        "if command -v head >/dev/null 2>&1; then head -n 200 \"${LLVM_COV_OUTPUT}\"; else cat \"${LLVM_COV_OUTPUT}\"; fi"
      COMMAND
        ${CMAKE_COMMAND} -E echo
        "[Coverage] Full LLVM coverage report available at ${LLVM_COV_OUTPUT}"
      COMMAND ${CMAKE_COMMAND} -E echo
              "[Coverage] LLVM LCOV export available at ${LLVM_COV_LCOV_OUTPUT}"
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Generating LLVM coverage report"
      VERBATIM USES_TERMINAL
      )
  endif()
  # Coverage summary target (prints summary to terminal)
  if(GCOVR_EXECUTABLE)
    set(_gcovr_summary_filter_paths ${PROJECT_SOURCE_DIR})
    get_filename_component(
      _gcovr_summary_project_real ${PROJECT_SOURCE_DIR} REALPATH
      )
    if(NOT _gcovr_summary_project_real STREQUAL ${PROJECT_SOURCE_DIR})
      list(APPEND _gcovr_summary_filter_paths ${_gcovr_summary_project_real})
    endif()
    set(_gcovr_summary_binary_dir ${CMAKE_BINARY_DIR}/${PROJECT_NAME})
    if(EXISTS ${_gcovr_summary_binary_dir})
      list(APPEND _gcovr_summary_filter_paths ${_gcovr_summary_binary_dir})
      get_filename_component(
        _gcovr_summary_binary_real ${_gcovr_summary_binary_dir} REALPATH
        )
      if(NOT _gcovr_summary_binary_real STREQUAL ${_gcovr_summary_binary_dir})
        list(APPEND _gcovr_summary_filter_paths ${_gcovr_summary_binary_real})
      endif()
    endif()
    list(REMOVE_DUPLICATES _gcovr_summary_filter_paths)
    set(GCOVR_SUMMARY_FILTER_ARGS)
    foreach(_gcovr_summary_filter_path IN LISTS _gcovr_summary_filter_paths)
      string(REGEX REPLACE "/$" "" _gcovr_summary_filter_trimmed
                           "${_gcovr_summary_filter_path}"
             )
      string(
        REGEX
        REPLACE [=[([][.^$+*?()|\])]=] [=[\\\1]=] _gcovr_summary_filter_escaped
                "${_gcovr_summary_filter_trimmed}"
        )
      list(APPEND GCOVR_SUMMARY_FILTER_ARGS --filter
           "${_gcovr_summary_filter_escaped}/.*"
           )
    endforeach()
    add_custom_target(
      coverage-summary
      COMMAND
        ${GCOVR_EXECUTABLE} --root ${PROJECT_SOURCE_DIR}
        ${GCOVR_SUMMARY_FILTER_ARGS} ${GCOVR_EXCLUDE_ARGS}
        --gcov-ignore-parse-errors=negative_hits.warn_once_per_file
        --gcov-ignore-errors=no_working_dir_found --print-summary
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Printing code coverage summary to terminal (gcovr)"
      VERBATIM COMMAND_EXPAND_LISTS
      )
    message(
      STATUS "Added 'coverage-summary' target for terminal summary output."
      )
  endif()

  set(_coverage_symlink_script
      "${PROJECT_SOURCE_DIR}/scripts/create_coverage_symlinks.py"
      )
  set(_coverage_symlink_root "${PROJECT_SOURCE_DIR}/.coverage-generated")
  if(Python3_FOUND AND EXISTS "${_coverage_symlink_script}")
    add_custom_target(
      coverage-symlink-prepare
      COMMAND ${Python3_EXECUTABLE} "${_coverage_symlink_script}" --build-root
              "${CMAKE_BINARY_DIR}" --output-root "${_coverage_symlink_root}"
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Preparing generated symlink tree (.coverage-generated)"
      VERBATIM
      )
  else()
    add_custom_target(
      coverage-symlink-prepare
      COMMAND ${CMAKE_COMMAND} -E rm -rf "${_coverage_symlink_root}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${_coverage_symlink_root}"
      COMMAND
        ${CMAKE_COMMAND} -E echo
        "WARNING: Python3 or create_coverage_symlinks.py missing; generated symlink tree will be empty."
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Preparing generated symlink tree (.coverage-generated)"
      )
  endif()

  set(_normalize_xml_script
      "${PROJECT_SOURCE_DIR}/scripts/normalize_coverage_xml.py"
      )
  set(_normalize_lcov_script
      "${PROJECT_SOURCE_DIR}/scripts/normalize_coverage_lcov.py"
      )

  add_custom_target(
    coverage
    COMMAND echo "Generating coverage reports from existing coverage data..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT
      "Generating coverage reports (use coverage-xml or coverage-html for specific formats)"
    )

  # HTML coverage report using lcov (if available)
  if(LCOV_EXECUTABLE AND GENHTML_EXECUTABLE)
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

    # HTML normalization target (depends on symlink preparation)
    if(Python3_FOUND AND EXISTS "${_normalize_lcov_script}")
      add_custom_target(
        coverage-html-normalize
        COMMAND
          ${Python3_EXECUTABLE} "${_normalize_lcov_script}" --repo-root
          "${PROJECT_SOURCE_DIR}" --coverage-root "${PROJECT_SOURCE_DIR}"
          --coverage-alias "${PROJECT_SOURCE_DIR}"
          "${CMAKE_BINARY_DIR}/coverage.info.final"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Normalizing LCOV HTML coverage report for editor/CI tooling"
        VERBATIM
        )
      add_dependencies(coverage-html-normalize coverage-symlink-prepare)
    else()
      add_custom_target(
        coverage-html-normalize
        COMMAND
          ${CMAKE_COMMAND} -E echo
          "ERROR: Python3 or normalize_coverage_lcov.py not found. Cannot normalize HTML coverage report."
        COMMAND exit 1
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Failed to normalize LCOV HTML coverage report"
        )
    endif()
    add_dependencies(coverage-html-normalize coverage-html)
    message(
      STATUS
        "Added 'coverage-html' target using lcov with filters: ${LCOV_EXTRACT_DESCRIPTION}"
      )
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
      string(REGEX REPLACE "/$" "" _gcovr_filter_trimmed
                           "${_gcovr_filter_path}"
             )
      string(REGEX REPLACE [=[([][.^$+*?()|\])]=] [=[\\\1]=]
                           _gcovr_filter_escaped "${_gcovr_filter_trimmed}"
             )
      list(APPEND GCOVR_FILTER_ARGS --filter "${_gcovr_filter_escaped}/.*")
    endforeach()

    add_custom_target(
      coverage-xml
      COMMAND
        ${GCOVR_EXECUTABLE} --root ${COVERAGE_SOURCE_ROOT} ${GCOVR_FILTER_ARGS}
        ${GCOVR_EXCLUDE_ARGS} --xml-pretty --exclude-unreachable-branches
        --print-summary
        --gcov-ignore-parse-errors=negative_hits.warn_once_per_file
        --gcov-ignore-errors=no_working_dir_found -o coverage.xml .
      COMMAND
        bash -c
        [=[set -euo pipefail; \
                  find "${CMAKE_BINARY_DIR}" -name '*.gcov.json.gz' -delete; \
                  find "${PROJECT_SOURCE_DIR}" -maxdepth 1 -name '*.gcov.json.gz' -delete]=]
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT
        "Generating XML coverage report with gcovr (root: ${COVERAGE_SOURCE_ROOT})"
      VERBATIM
      )

    # XML normalization target (depends on symlink preparation)
    if(Python3_FOUND AND EXISTS "${_normalize_xml_script}")
      add_custom_target(
        coverage-xml-normalize
        COMMAND
          ${Python3_EXECUTABLE} "${_normalize_xml_script}" --repo-root
          "${PROJECT_SOURCE_DIR}" --source-dir "${PROJECT_SOURCE_DIR}"
          --path-map
          "${CMAKE_BINARY_DIR}=${PROJECT_SOURCE_DIR}/.coverage-generated"
          "${CMAKE_BINARY_DIR}/coverage.xml"
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Normalizing XML coverage report for editor/CI tooling"
        VERBATIM
        )
      add_dependencies(coverage-xml-normalize coverage-symlink-prepare)
    else()
      add_custom_target(
        coverage-xml-normalize
        COMMAND
          ${CMAKE_COMMAND} -E echo
          "ERROR: Python3 or normalize_coverage_xml.py not found. Cannot normalize XML coverage report."
        COMMAND exit 1
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Failed to normalize XML coverage report"
        )
    endif()
    add_dependencies(coverage-xml-normalize coverage-xml)
    message(
      STATUS
        "Added 'coverage-xml' target using gcovr with root: ${COVERAGE_SOURCE_ROOT}"
      )

    # Symlink cleanup target (can be used independently)
    add_custom_target(
      coverage-symlink-clean
      COMMAND ${CMAKE_COMMAND} -E rm -rf "${_coverage_symlink_root}"
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Cleaning up generated symlink tree (.coverage-generated)"
      )

    set(_coverage_gcov_dependencies coverage-symlink-prepare coverage-xml
                                    coverage-summary
        )
    if(TARGET coverage-xml-normalize)
      list(APPEND _coverage_gcov_dependencies coverage-xml-normalize)
    endif()
    if(TARGET coverage-html)
      list(APPEND _coverage_gcov_dependencies coverage-html)
    endif()
    if(TARGET coverage-html-normalize)
      list(APPEND _coverage_gcov_dependencies coverage-html-normalize)
    endif()

    add_custom_target(
      coverage-gcov
      DEPENDS ${_coverage_gcov_dependencies}
      COMMAND
        ${CMAKE_COMMAND} -E echo
        "[Coverage] gcovr XML report available at ${CMAKE_BINARY_DIR}/coverage.xml"
      COMMAND
        bash -c
        "if [ -d \"${CMAKE_BINARY_DIR}/coverage-html\" ]; then \
                   echo '[Coverage] HTML report directory: ${CMAKE_BINARY_DIR}/coverage-html'; \
                 else \
                   echo '[Coverage] HTML report directory not generated (lcov/genhtml unavailable).'; \
                 fi"
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Generating GCC coverage reports"
      VERBATIM USES_TERMINAL
      )
    add_dependencies(coverage coverage-gcov)
  endif()

  add_custom_target(
    coverage-clean
    COMMAND find ${CMAKE_BINARY_DIR} -name "*.gcda" -delete
    COMMAND find ${CMAKE_BINARY_DIR} -name "*.gcno" -delete
    COMMAND rm -f ${CMAKE_BINARY_DIR}/coverage.info*
    COMMAND rm -f ${CMAKE_BINARY_DIR}/coverage.xml
    COMMAND rm -rf ${CMAKE_BINARY_DIR}/coverage-html
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Cleaning coverage data files"
    )

  message(
    STATUS
      "Coverage targets added: coverage, coverage-gcov, coverage-xml, coverage-html, coverage-clean"
    )
endfunction()
