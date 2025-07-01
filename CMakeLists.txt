cmake_minimum_required(VERSION 3.31)

# ##############################################################################
# Ensure we have access to a suitable version of Cetmodules
include(FetchContent)
FetchContent_Declare(
  cetmodules
  GIT_REPOSITORY https://github.com/FNALssi/cetmodules
  GIT_TAG v4-branch
  # GIT_TAG fd6ebf45
  FIND_PACKAGE_ARGS 3.99.00
  )
# ... and Microsoft's C++ Guideline Support Library
FetchContent_Declare(
  GSL
  GIT_REPOSITORY https://github.com/microsoft/GSL
  GIT_TAG 2828399820ef4928cc89b65605dca5dc68efca6e # v4.2.0
  GIT_SHALLOW ON
  FIND_PACKAGE_ARGS NAMES Microsoft.GSL
  )
# ... and Catch2
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG 914aeecfe23b1e16af6ea675a4fb5dbd5a5b8d0a # v3.8.0
  GIT_SHALLOW ON
  FIND_PACKAGE_ARGS
  )
FetchContent_MakeAvailable(cetmodules Catch2 GSL)
# ##############################################################################

find_package(cetmodules 3.99.00 REQUIRED)
project(
  phlex
  VERSION 0.0.1
  LANGUAGES CXX
  )

option(ENABLE_TSAN "Enable Thread Sanitizer" OFF)
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(PHLEX_USE_FORM "Enable experimental integration with FORM" OFF)

add_compile_options(-Wall -Werror -Wunused -Wunused-parameter -pedantic)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "14.1"
    )
    # GCC 14.1 issues many false positives re. array-bounds and stringop-overflow
    add_compile_options(-Wno-array-bounds -Wno-stringop-overflow)
  endif ()
endif ()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)

# add_compile_options(-fprofile-instr-generate -fcoverage-mapping)

list(PREPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/Modules)

find_library(BACKTRACE_LIB backtrace REQUIRED)
find_path(BACKTRACE_INC backtrace.h REQUIRED)
find_package(Boost REQUIRED COMPONENTS json program_options stacktrace_basic)
find_package(TBB REQUIRED)
find_package(fmt REQUIRED)
find_package(jsonnet REQUIRED)
find_package(spdlog REQUIRED)

# Apply ThreadSanitizer flags if enabled
if (ENABLE_TSAN)
  # Check if the compiler supports ThreadSanitizer
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang"
      OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang"
      OR CMAKE_CXX_COMPILER_ID MATCHES "GNU"
      )
    message(STATUS "Enabling ThreadSanitizer")
    # Add the sanitizer flag
    add_compile_options(-fsanitize=thread -g -O1
      # Ensure no optimizations interfere with TSan
      $<$<COMPILER_LANGUAGE:CXX>:-fno-omit-frame-pointer -fno-optimize-sibling-calls>
    )
    add_link_options(-fsanitize=thread)
  else ()
    message(
      FATAL_ERROR
        "ThreadSanitizer is not supported with ${CMAKE_CXX_COMPILER_ID}"
      )
  endif ()
endif ()

if (ENABLE_ASAN)
  # Check if the compiler supports AddressSanitizer
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang"
      OR CMAKE_CXX_COMPILER_ID MATCHES "AppleClang"
      OR CMAKE_CXX_COMPILER_ID MATCHES "GNU"
      )
    message(STATUS "Enabling AddressSanitizer")
    # Add the sanitizer flag
    add_compile_options(-fsanitize=address -g -O1
      # Ensure no optimizations interfere with TSan
      $<$<COMPILER_LANGUAGE:CXX>:-fno-omit-frame-pointer -fno-optimize-sibling-calls>
    )
    add_link_options(-fsanitize=address)
  else ()
    message(
      FATAL_ERROR
        "AddressSanitizer is not supported with ${CMAKE_CXX_COMPILER_ID}"
      )
  endif ()
endif ()

add_subdirectory(phlex)

if (PHLEX_USE_FORM)
  add_subdirectory(form)
endif()

include(CTest)
if (BUILD_TESTING)
  add_subdirectory(test)
endif()
