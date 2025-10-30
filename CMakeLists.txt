cmake_minimum_required(VERSION 3.31)

# ##############################################################################
# Ensure we have access to ...
include(FetchContent)
# ... the Catch2 unit testing framework
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.10.0
  GIT_SHALLOW ON
  FIND_PACKAGE_ARGS
  )
# ... Microsoft's C++ Guideline Support Library
FetchContent_Declare(
  GSL
  GIT_REPOSITORY https://github.com/microsoft/GSL
  GIT_TAG v4.2.0
  GIT_SHALLOW ON
  FIND_PACKAGE_ARGS NAMES Microsoft.GSL
  )
# ... the Mimic C++ mocking framework
FetchContent_Declare(
  mimicpp
  GIT_REPOSITORY https://github.com/DNKpp/mimicpp
  GIT_TAG v8
  GIT_SHALLOW ON
  FIND_PACKAGE_ARGS
  )

# Need Cetmodules _before_ first project() declaration
find_package(cetmodules 4.01.01 REQUIRED)

# ##############################################################################
# Main project declaration
project(
  phlex
  VERSION 0.0.1
  LANGUAGES CXX
  )
# ##############################################################################

# Make tools available
FetchContent_MakeAvailable(Catch2 GSL mimicpp)

include(Modules/private/CreateClangTidyTargets.cmake)
include(Modules/private/CreateCoverageTargets.cmake)

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
    create_coverage_targets()
  endif()
endif()

# Create clang-tidy targets
create_clang_tidy_targets()
