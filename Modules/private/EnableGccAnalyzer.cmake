# --- EnableGccAnalyzer.cmake
#
# This module checks if the ENABLE_GCC_ANALYZER option is enabled, and if so,
# it adds the necessary compiler flags to enable the GCC static analyzer.

if(ENABLE_GCC_ANALYZER)
  # Check if the compiler is GNU
  if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(FATAL_ERROR "GCC Static Analyzer is only supported with the GNU compiler.")
  else()
    message(STATUS "Enabling GCC Static Analyzer")
    add_compile_options(-fanalyzer)
  endif()
endif()
