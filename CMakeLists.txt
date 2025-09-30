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
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION
                                            VERSION_GREATER_EQUAL "14.1"
   )
  add_compile_options(-Wno-array-bounds -Wno-stringop-overflow)
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
    add_compile_options(-fsanitize=thread -g -O1)
    add_link_options(-fsanitize=thread)
    # Ensure no optimizations interfere with TSan
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fno-optimize-sibling-calls"
        )
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
    add_compile_options(-fsanitize=address -g -O1)
    add_link_options(-fsanitize=address)
    # Ensure no optimizations interfere with TSan
    set(CMAKE_CXX_FLAGS
        "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fno-optimize-sibling-calls"
        )
  else()
    message(
      FATAL_ERROR
        "AddressSanitizer is not supported with ${CMAKE_CXX_COMPILER_ID}"
      )
  endif()
endif()

# Configure clang-tidy integration
option(ENABLE_CLANG_TIDY "Enable clang-tidy checks during build" OFF)

if (ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy-20 clang-tidy)

  if (CLANG_TIDY_EXECUTABLE)
    message(STATUS "Found clang-tidy: ${CLANG_TIDY_EXECUTABLE}")
    set(CMAKE_CXX_CLANG_TIDY
        ${CLANG_TIDY_EXECUTABLE}
        --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy)
  else ()
    message(WARNING "clang-tidy not found, disabling clang-tidy checks")
  endif ()
endif ()

add_subdirectory(phlex)

if(PHLEX_USE_FORM)
  add_subdirectory(form)
endif()

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(test)
endif()

# ============================================================================
# Clang-Tidy targets
# ============================================================================

# Find clang-tidy for custom targets (independent of ENABLE_CLANG_TIDY option)
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy-20 clang-tidy)

if (CLANG_TIDY_EXECUTABLE)
  # Get all C++ source files from the phlex target
  get_target_property(PHLEX_SOURCES phlex SOURCES)

  # Filter to only C++ source files (exclude headers)
  set(PHLEX_CXX_SOURCES)
  foreach(source ${PHLEX_SOURCES})
    if(source MATCHES "\\.(cpp|cxx|cc)$")
      list(APPEND PHLEX_CXX_SOURCES ${source})
    endif()
  endforeach()

  # Clang-tidy check target (read-only check)
  add_custom_target(clang-tidy-check
    COMMAND ${CLANG_TIDY_EXECUTABLE}
      --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
      -p ${CMAKE_BINARY_DIR}
      ${PHLEX_CXX_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running clang-tidy checks on phlex sources"
    VERBATIM
  )

  # Clang-tidy fix target (applies fixes)
  add_custom_target(clang-tidy-fix
    COMMAND ${CLANG_TIDY_EXECUTABLE}
      --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
      -p ${CMAKE_BINARY_DIR}
      --fix
      --fix-errors
      ${PHLEX_CXX_SOURCES}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Applying clang-tidy fixes to phlex sources"
    VERBATIM
  )

  message(STATUS "Clang-tidy targets added: clang-tidy-check, clang-tidy-fix (using ${CLANG_TIDY_EXECUTABLE})")
else()
  message(STATUS "clang-tidy not found, skipping clang-tidy targets")
endif()
