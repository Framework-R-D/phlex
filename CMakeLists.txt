cmake_minimum_required(VERSION 3.31)

# ##############################################################################
# Ensure we have access to a suitable version of Cetmodules
include(FetchContent)
FetchContent_Declare(
  cetmodules
  GIT_REPOSITORY https://github.com/FNALssi/cetmodules
  GIT_TAG 4.01.01
  GIT_SHALLOW ON
  FIND_PACKAGE_ARGS 4.01.01
  )
# ... and the Catch2 unit testing framework
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.10.0
  GIT_SHALLOW ON
  FIND_PACKAGE_ARGS
  )
# ... and Microsoft's C++ Guideline Support Library
FetchContent_Declare(
  GSL
  GIT_REPOSITORY https://github.com/microsoft/GSL
  GIT_TAG v4.2.0
  GIT_SHALLOW ON
  FIND_PACKAGE_ARGS NAMES Microsoft.GSL
  )
# ... and the Mimic C++ mocking framework
FetchContent_Declare(
  mimicpp
  GIT_REPOSITORY https://github.com/DNKpp/mimicpp
  GIT_TAG v8
  GIT_SHALLOW ON
  FIND_PACKAGE_ARGS
  )

# Make cetmodules available
FetchContent_MakeAvailable(cetmodules)
find_package(cetmodules 4.01.01 REQUIRED)

# ##############################################################################
# Main project declaration
project(
  phlex
  VERSION 0.0.1.1
  LANGUAGES CXX
  )
# ##############################################################################

# Make tools available
FetchContent_MakeAvailable(Catch2 GSL mimicpp)

include(Modules/private/CreateClangTidyTargets.cmake)

option(ENABLE_TSAN "Enable Thread Sanitizer" OFF)
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(PHLEX_USE_FORM "Enable experimental integration with FORM" OFF)
option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)
option(ENABLE_CLANG_TIDY "Enable clang-tidy checks during build" OFF)

add_compile_options(-Wall -Werror -Wunused -Wunused-parameter -pedantic)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "14.1"
     AND CMAKE_COMPILER_VERSION VERSION_LESS "15"
     )
    # GCC 14.1 issues many false positives re. array-bounds and
    # stringop-overflow
    add_compile_options(-Wno-array-bounds -Wno-stringop-overflow)
  endif()
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)

# add_compile_options(-fprofile-instr-generate -fcoverage-mapping)

list(PREPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/Modules)

find_package(Boost REQUIRED COMPONENTS json program_options)
find_package(TBB REQUIRED)
find_package(fmt REQUIRED)
find_package(jsonnet REQUIRED)
find_package(spdlog REQUIRED)

# Apply ThreadSanitizer flags if enabled
if(ENABLE_TSAN)
  # Check if the compiler supports ThreadSanitizer
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang"
     OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang"
     OR CMAKE_CXX_COMPILER_ID MATCHES "GNU"
     )
    message(STATUS "Enabling ThreadSanitizer")
    # Add the sanitizer flag
    add_compile_options(
      -fsanitize=thread
      -g
      -O1
      # Ensure no optimizations interfere with TSan
      $<$<COMPILER_LANGUAGE:CXX>:-fno-omit-frame-pointer
      -fno-optimize-sibling-calls>
      )
    add_link_options(-fsanitize=thread)
  else()
    message(
      FATAL_ERROR
        "ThreadSanitizer is not supported with ${CMAKE_CXX_COMPILER_ID}"
      )
  endif()
endif()

if(ENABLE_ASAN)
  # Check if the compiler supports AddressSanitizer
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang"
     OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang"
     OR CMAKE_CXX_COMPILER_ID MATCHES "GNU"
     )
    message(STATUS "Enabling AddressSanitizer")
    # Add the sanitizer flag
    add_compile_options(
      -fsanitize=address
      -g
      -O1
      # Ensure no optimizations interfere with ASan
      $<$<COMPILER_LANGUAGE:CXX>:-fno-omit-frame-pointer
      -fno-optimize-sibling-calls>
      )
    add_link_options(-fsanitize=address)
  else()
    message(
      FATAL_ERROR
        "AddressSanitizer is not supported with ${CMAKE_CXX_COMPILER_ID}"
      )
  endif()
endif()

# Configure code coverage if enabled
if(ENABLE_COVERAGE)
  # Check if the compiler supports code coverage
  if(CMAKE_CXX_COMPILER_ID MATCHES "GNU"
     OR CMAKE_CXX_COMPILER_ID MATCHES "Clang"
     OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang"
     )
    message(STATUS "Enabling code coverage instrumentation")

    # Use CMake's built-in coverage flags Force these into the cache to override
    # any empty defaults
    set(CMAKE_CXX_FLAGS_COVERAGE
        "-g -O0 --coverage -fprofile-arcs -ftest-coverage"
        CACHE STRING "C++ flags for Coverage build" FORCE
        )
    set(CMAKE_C_FLAGS_COVERAGE
        "-g -O0 --coverage -fprofile-arcs -ftest-coverage"
        CACHE STRING "C flags for Coverage build" FORCE
        )
    set(CMAKE_EXE_LINKER_FLAGS_COVERAGE
        "--coverage"
        CACHE STRING "Executable linker flags for Coverage build" FORCE
        )
    set(CMAKE_SHARED_LINKER_FLAGS_COVERAGE
        "--coverage"
        CACHE STRING "Shared library linker flags for Coverage build" FORCE
        )
    set(CMAKE_MODULE_LINKER_FLAGS_COVERAGE
        "--coverage"
        CACHE STRING "Module linker flags for Coverage build" FORCE
        )

    # Set the build type to Coverage if not specified
    if(NOT CMAKE_BUILD_TYPE)
      set(CMAKE_BUILD_TYPE
          Coverage
          CACHE STRING "Build type" FORCE
          )
    endif()

    # Configure CTest to find coverage tools automatically
    find_program(
      CTEST_COVERAGE_COMMAND
      NAMES gcov llvm-cov
      DOC "Coverage tool for CTest"
      )

    if(CTEST_COVERAGE_COMMAND)
      message(STATUS "Found coverage tool: ${CTEST_COVERAGE_COMMAND}")
    else()
      message(WARNING "Coverage tool not found - coverage reports may not work")
    endif()

  else()
    message(
      FATAL_ERROR "Code coverage is not supported with ${CMAKE_CXX_COMPILER_ID}"
      )
  endif()
endif()

# Configure clang-tidy integration
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy-20 clang-tidy)

if(ENABLE_CLANG_TIDY)
  if(CLANG_TIDY_EXECUTABLE)
    message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXECUTABLE}")
    set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY_EXECUTABLE}
                             --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
        )
  else()
    message(WARNING "clang-tidy not found, disabling clang-tidy checks")
  endif()
endif()

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE
      RelWithDebInfo
      CACHE STRING "Build type" FORCE
      )
endif()

add_subdirectory(phlex)

if(PHLEX_USE_FORM)
  set(BUILD_SHARED_LIBS OFF) # Temporary
  add_subdirectory(form)
  unset(BUILD_SHARED_LIBS)
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(test)

  # Add coverage targets if coverage is enabled
  if(ENABLE_COVERAGE)
    # Find coverage report generation tools
    find_program(LCOV_EXECUTABLE lcov)
    find_program(GENHTML_EXECUTABLE genhtml)
    find_program(GCOVR_EXECUTABLE gcovr)

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
      COMMAND rm -f ${CMAKE_BINARY_DIR}/coverage.info*
      COMMAND rm -f ${CMAKE_BINARY_DIR}/coverage.xml
      COMMAND rm -rf ${CMAKE_BINARY_DIR}/coverage_html
      WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
      COMMENT "Cleaning coverage data files"
      )

    message(
      STATUS
        "Coverage targets added: coverage, coverage-xml, coverage-html, coverage-clean"
      )
  endif()
endif()

# Create clang-tidy targets
create_clang_tidy_targets()
