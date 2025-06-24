cmake_minimum_required(VERSION 3.22)

project(
  phlex
  VERSION 0.0.1
  LANGUAGES CXX
  )

option(ENABLE_TSAN "Enable Thread Sanitizer" OFF)
option(ENABLE_ASAN "Enable Address Sanitizer" OFF)
option(PHLEX_USE_FORM "Enable experimental integration with FORM" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE "RelWithDebInfo")
# set(CMAKE_BUILD_TYPE "Debug")
add_compile_options(-Wall -Werror -Wunused -Wunused-parameter -pedantic)

# GCC 14.1 issues many false positives re. array-bounds and stringop-overflow
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION
                                             VERSION_GREATER_EQUAL "14.1"
    )
  add_compile_options(-Wno-array-bounds -Wno-stringop-overflow)
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
    add_compile_options(-fsanitize=thread -g -O1)
    add_link_options(-fsanitize=thread)
    # Ensure no optimizations interfere with TSan
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fno-optimize-sibling-calls"
        )
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
    add_compile_options(-fsanitize=address -g -O1)
    add_link_options(-fsanitize=address)
    # Ensure no optimizations interfere with TSan
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fno-optimize-sibling-calls"
        )
  else ()
    message(
      FATAL_ERROR
        "AddressSanitizer is not supported with ${CMAKE_CXX_COMPILER_ID}"
      )
  endif ()
endif ()

add_subdirectory(phlex)

include(CTest)
if (BUILD_TESTING)
  add_subdirectory(test)
endif()
