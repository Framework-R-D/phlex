#[================================================================[.rst:
X
-
#]================================================================]
# ##############################################################################
# cet_set_compiler_flags( [<options>] )
#
# Options:
#
# DIAGS <diag-level>
#
# This option may be CAVALIER, CAUTIOUS, VIGILANT or PARANOID. Default is
# CAUTIOUS.
#
# DWARF_STRICT
#
# Instruct the compiler not to emit any debugging information more advanced than
# that selected. This will prevent possible errors in older debuggers, but may
# prevent certain C++11 constructs from being debuggable in modern debuggers.
#
# DWARF_VER <#>
#
# Version of the DWARF standard to use for generating debugging information.
# Default depends upon the compiler: GCC v4.8.0 and above emit DWARF4 by
# default; earlier compilers emit DWARF2.
#
# ENABLE_ASSERTS
#
# Enable asserts regardless of CMAKE_BUILD_TYPE (default is to disable asserts
# for all but Debug).
#
# EXTRA_FLAGS <flags> EXTRA_C_FLAGS <flags> (DEPRECATED) EXTRA_CXX_FLAGS <flags>
# (DEPRECATED) EXTRA_DEFINITIONS <flags>
#
# These list parameters will append the specified flags via
# add_compile_options() or add_compile_definitions(). EXTRA_<lang>_FLAGS options
# are deprecated: use EXTRA_FLAGS with generator expressions instead e.g.
# EXTRA_FLAGS $<$<COMPILE_LANGUAGE:<lang>[,<lang>]>:<flags>>
#
# NO_UNDEFINED
#
# Unresolved symbols will cause an error when making a shared library.
#
# WERROR
#
# All warnings are flagged as errors unless countermanded with -Werror=no-<diag>
# or -Wno-<diag>.
#
# ##############################################################################
# cet_enable_asserts()
#
# Enable use of assserts (ie remove -DNDEBUG) regardless of optimization level.
#
# ##############################################################################
# cet_disable_asserts()
#
# Disable use of assserts (ie ensure -DNDEBUG) regardless of optimization level.
#
# ##############################################################################
# cet_maybe_disable_asserts()
#
# Possibly disable use of assserts (ie ensure -DNDEBUG) based on optimization
# level.
#
# ##############################################################################
# cet_add_compiler_flags(<options> <flags>...)
#
# Add the specified compiler flags (DEPRECATED).
#
# Options:
#
# C|CXX
#
# Identical to LANGUAGES <lang>
#
# LANGUAGES <lang>...
#
# Add <flags> via add_compile_options for <lang>...
#
# Notes:
#
# * If no languages are specified, we default to C and CXX.
#
# * Duplicates are not removed.
#
# * This function is deprecated: use the modern CMake function
#   add_compile_options() instead with generator expressions.
#
# ##############################################################################
# cet_remove_compiler_flags([C] [CXX] [LANGUAGES ...] <flag>...)
#
# Remove <flag>... from specified CMake languages (DEPRECATED).
#
# Options:
#
# C|CXX
#
# Identical to LANGUAGES <lang>.
#
# LANGUAGES <X>
#
# Remove <flags> from CMAKE_<X>_FLAGS.
#
# Notes:
#
# * This function will only remove explicit flags from CMake's global variables
#   (CMAKE_CXX_COMPILER_FLAGS, etc.), and definitions (-D, -U) with scoped
#   remove_definitions() commands. It cannot preclude the same options being
#   added back in some other way, nor can it remove flags set in properties, nor
#   configuration- or language-specific options added via generator expressions.
#
# * If no languages are specified, we default to C CXX.
#
# ##############################################################################

# Avoid unnecessary repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

include(CetPackagePath)
include(CetRegexEscape)

function(cet_report_compiler_flags)
  cmake_parse_arguments(PARSE_ARGV 0 CRCF "" "REPORT_THRESHOLD" "")
  if(NOT CRCF_REPORT_THRESHOLD)
    set(CRCF_REPORT_THRESHOLD STATUS)
  endif()
  set(gen_exps)
  cet_package_path(current_subdir HUMAN_READABLE)
  message(${CRCF_REPORT_THRESHOLD}
          "Global CMAKE_<lang>_FLAGS* variables in the current scope"
          " (${current_subdir}) for CMAKE_BUILD_TYPE = ${CMAKE_BUILD_TYPE}:"
          )
  get_property(
    vars
    DIRECTORY
    PROPERTY VARIABLES
    )
  set(flags_regex "^CMAKE_([^_]+)_FLAGS(_[^_]+)?$")
  list(FILTER vars INCLUDE REGEX "${flags_regex}")
  list(TRANSFORM vars REPLACE "${flags_regex}" "\\1" OUTPUT_VARIABLE langs)
  list(REMOVE_DUPLICATES langs)
  foreach(lang IN LISTS langs)
    if(CMAKE_${lang}_COMPILER_ID)
      list(REMOVE_ITEM langs "${lang}")
    endif()
  endforeach()
  cet_regex_escape(VAR langs "${langs}")
  list(JOIN "|" langs lang_regex)
  list(FILTER vars EXCLUDE REGEX "^CMAKE_(${lang_regex})_FLAGS")
  foreach(var IN LISTS vars)
    if(${var} MATCHES [[$<]])
      list(APPEND gen_exps "${var}")
    endif()
    message(${CRCF_REPORT_THRESHOLD} "  ${var} = ${${var}}")
  endforeach()
  message(${CRCF_REPORT_THRESHOLD} "These may be overridden by:")
  foreach(prop IN ITEMS COMPILE_DEFINITIONS COMPILE_OPTIONS INCLUDE_DIRECTORIES
                        LINK_OPTIONS LINK_DIRECTORIES
          )
    get_property(
      items
      DIRECTORY
      PROPERTY ${prop}
      )
    if(items)
      if(items MATCHES [[$<]])
        list(APPEND gen_exps ${prop})
      endif()
      message(${CRCF_REPORT_THRESHOLD}
              "  * property ${prop} for the current scope:"
              )
      foreach(
        item IN
        LISTS
        items
        )
        message(${CRCF_REPORT_THRESHOLD} "      ${item}")
      endforeach()
    endif()
  endforeach()
  message(${CRCF_REPORT_THRESHOLD}
          "  * properties modified at lower-level directories"
          )
  message(${CRCF_REPORT_THRESHOLD}
          "  * target- and source-specific directories."
          )
  if(gen_exps)
    list(JOIN gen_exps ", " gen_exps_string)
    message(
      ${CRCF_REPORT_THRESHOLD}
      "some variables or properties (${gen_exps_string})"
      " contain generator expressions (\$<...>) and will be resolved later."
      )
    message(${CRCF_REPORT_THRESHOLD}
            "build with generator verbosity to see actual definitions and"
            " flags used for each target."
            )
  endif()
endfunction()

macro(cet_enable_asserts)
  remove_definitions(-DNDEBUG -UNDEBUG)
  add_compile_options(-UNDEBUG)
endmacro()

macro(cet_disable_asserts)
  remove_definitions(-DNDEBUG -UNDEBUG)
  add_compile_definitions(NDEBUG)
endmacro()

macro(cet_maybe_disable_asserts)
  remove_definitions(-DNDEBUG -UNDEBUG)
  add_compile_definitions($<$<NOT:$<CONFIG:DEBUG>>:NDEBUG>) # Conditional.
endmacro()

function(_parse_flags_options)
  cmake_parse_arguments(PARSE_ARGV 0 CSCF "C;CXX;QUIET" "" "LANGUAGES")
  if(CSCF_C)
    list(APPEND CSCF_LANGUAGES "C")
  endif()
  if(CSCF_CXX)
    list(APPEND CSCF_LANGUAGES "CXX")
  endif()
  if(NOT CSCF_LANGUAGES)
    set(CSCF_LANGUAGES C CXX)
  endif()
  foreach(var IN ITEMS C CXX KEYWORDS_MISSING_VALUES QUIET LANGUAGES
                       UNPARSED_ARGUMENTS
          )
    if(DEFINED CSCF_${var})
      set(CSCF_${var}
          "${CSCF_${var}}"
          PARENT_SCOPE
          )
    endif()
  endforeach()
endfunction()

function(cet_add_compiler_flags)
  _parse_flags_options(${ARGN})
  if("${CSCF_ARGS}" MATCHES "(^| )-std=" AND ("C" IN_LIST CSCF_LANGUAGES
                                              OR "CXX" IN_LIST CSCF_LANGUAGES)
     )
    message(
      FATAL_ERROR
        "cet_add_compiler_flags() called with -std=...for C and/or CXX:"
        "use CMAKE_<LANG>_STANDARD and CMAKE_<LANG>_EXTENSIONS instead"
      )
  endif()
  string(REPLACE ";" "," _cet_languages "${CSCF_LANGUAGES}")
  add_compile_options(
    "SHELL:$<$<COMPILE_LANGUAGE:${_cet_languages}>:${CSCF_UNPARSED_ARGUMENTS}>"
    )
endfunction()

function(cet_remove_compiler_flags)
  _parse_flags_options(${ARGN})
  cmake_parse_arguments(CSCF "REGEX" "" "" ${CSCF_UNPARSED_ARGUMENTS})
  get_property(
    vars
    DIRECTORY
    PROPERTY VARIABLES
    )
  cet_regex_escape(VAR langs "${CSCF_LANGUAGES}")
  list(JOIN langs "|" langs_regex)
  list(FILTER vars INCLUDE REGEX "^CMAKE_(${langs_regex})_FLAGS")
  foreach(arg IN LISTS CSCF_UNPARSED_ARGUMENTS)
    if(NOT CSCF_REGEX)
      if(arg MATCHES "^-[DU]")
        remove_definitions(${arg})
      endif()
      cet_regex_escape("${arg}" arg)
    endif()
    foreach(flags_var IN LISTS vars)
      string(REGEX REPLACE "(^| +)${arg}( *|$)" " " ${flags_var}
                           "${${flags_var}}"
             )
    endforeach()
  endforeach()
  foreach(flags_var IN LISTS vars)
    string(STRIP ${flags_var} "${${flags_var}}")
    set(${flags_var}
        "${${flags_var}}"
        PARENT_SCOPE
        )
  endforeach()
  warn_deprecated(
    "cet_remove_compiler_flags()" NEW "add_compiler_flags() with negated flags"
    )
endfunction()

macro(cet_set_compiler_flags)
  cmake_parse_arguments(
    CSCF
    "ALLOW_DEPRECATIONS;DWARF_STRICT;ENABLE_ASSERTS;NO_UNDEFINED;WERROR"
    ""
    "DIAGS;DWARF_VER;EXTRA_FLAGS;EXTRA_C_FLAGS;EXTRA_CXX_FLAGS;EXTRA_DEFINITIONS"
    ${ARGN}
    )

  # No non-option arguments.
  if(CSCF_UNPARSED_ARGUMENTS)
    message(
      FATAL_ERROR "Unexpected extra arguments: ${CSCF_UNPARSED_ARGUMENTS}."
                  "\nConsider EXTRA_FLAGS or EXTRA_DEFINITIONS"
      )
  endif()

  # WERROR.
  if(CSCF_WERROR)
    add_compile_options(-Werror)
  endif()

  # ALLOW_DEPRECATIONS.
  if(CSCF_ALLOW_DEPRECATIONS)
    add_compile_options(-Wno-error=deprecated-declarations)
  endif()

  # DWARF_VER, DWARF_STRICT.
  if(CSCF_DWARF_VER)
    if(CSCF_DWARF_VER LESS 4)
      message(WARNING "Setting DWARF format version < 4 may impact your"
                      " ability to debug modern C++ programs."
              )
    endif()
    add_compile_options(-gdwarf-${CSCF_DWARF_VER})
    if(CSCF_DWARF_STRICT)
      add_compile_options(-gstrict-dwarf)
    endif()
  endif()

  if(CSCF_ENABLE_ASSERTS)
    cet_enable_asserts()
  else()
    cet_maybe_disable_asserts()
  endif()

  # EXTRA_DEFINITIONS.
  #
  # Note that we no longer want the leading "-D", and -U... undefines must be
  # filtered out and handled via add_compile_options() instead.
  list(TRANSFORM CSCF_EXTRA_DEFINITIONS
       REPLACE "^-D([A-Za-z_][A-Za-z_0-9]*)$" "\\1" OUTPUT_VARIABLE
                                                    compile_defs
       )
  list(FILTER CSCF_EXTRA_DEFINITIONS INCLUDE REGEX "^-U")
  list(FILTER compile_defs EXCLUDE REGEX "^-U")

  add_compile_definitions(${compile_defs})
  add_compile_options(${CSCF_EXTRA_DEFINITIONS})

  # Generally-useful options.
  add_compile_options(
    "SHELL:$<$<COMPILE_LANG_AND_ID:$<COMPILE_LANGUAGE>,GNU>:-frecord-gcc-switches>"
    )
  add_compile_options(
    "SHELL:$<$<COMPILE_LANG_AND_ID:$<COMPILE_LANGUAGE>,Clang,AppleClang,GNU>:-grecord-gcc-switches>"
    )

  # Add options according to diagnostic mode DIAGS.
  set(diags_vals CAVALIER CAUTIOUS VIGILANT PARANOID)
  string(TOUPPER "${CSCF_DIAGS}" CSCF_DIAGS)
  if(NOT CSCF_DIAGS)
    set(CSCF_DIAGS "CAUTIOUS")
  endif()
  list(FIND diags_vals ${CSCF_DIAGS} diag_idx)
  if(diag_idx GREATER -1)
    message(VERBOSE "selected diagnostic level: ${CSCF_DIAGS}")
    if(diag_idx GREATER 0) # At least CAUTIOUS
      add_compile_options(
        "SHELL:$<$<COMPILE_LANGUAGE:C,CXX>:-Wall -Werror=return-type>"
        )
      if(diag_idx GREATER 1) # At least VIGILANT
        add_compile_options(-Wextra -Wno-long-long -Winit-self)
        add_compile_options(
          "SHELL:$<$<AND:$<COMPILE_LANGUAGE:C,CXX>,$<OR:$<COMPILE_LANG_AND_ID:$<COMPILE_LANGUAGE>,Clang,AppleClang>,$<AND:$<COMPILE_LANG_AND_ID:$<COMPILE_LANGUAGE>,GNU>,$<VERSION_GREATER_EQUAL:$<$<COMPILE_LANGUAGE>_COMPILER_VERSION>,4.7.0>>>>:-Wno-unused-local-typedefs>"
          )
        add_compile_options(
          "SHELL:$<$<COMPILE_LANGUAGE:CXX>:-Wdelete-non-virtual-dtor>"
          ) # C++ only
        add_compile_options(
          "SHELL:$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<OR:$<CXX_COMPILER_ID:Clang,AppleClang>,$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,4.7.0>>>>:-Woverloaded-virtual -Wnon-virtual-dtor>"
          ) # C++ only
        if(diag_idx GREATER 2) # PARANOID
          add_compile_options(
            -pedantic
            -Wformat-y2k
            -Wswitch-default
            -Wsync-nand
            -Wtrampolines
            -Wlogical-op
            -Wshadow
            -Wcast-qual
            )
        endif(diag_idx GREATER 2)
      endif(diag_idx GREATER 1)
    endif(diag_idx GREATER 0)
  else()
    message(FATAL_ERROR "Unrecognized DIAGS option ${CSCF_DIAGS}")
  endif()

  # EXTRA(_C|_CXX)?_FLAGS.
  foreach(lang IN ITEMS C CXX)
    if(CSCF_EXTRA_${lang}_FLAGS)
      warn_deprecated(
        "EXTRA_${lang}_FLAGS" NEW "EXTRA_FLAGS with generator expressions"
        )
      list(APPEND CSCF_EXTRA_FLAGS
           "$<$<COMPILE_LANGUAGE:${lang}>:${CSCF_EXTRA_${lang}_FLAGS}>"
           )
    endif()
  endforeach()
  add_compile_options("SHELL:${CSCF_EXTRA_FLAGS}")

  # NO_UNDEFINED.
  if(CSCF_NO_UNDEFINED)
    add_link_options(
      "SHELL:$<IF:$<PLATFORM_ID:Darwin>,LINKER:-undefined$<COMMA>error,LINKER:--unresolved-symbols=ignore-in-shared-libs>"
      )
  else()
    add_link_options(
      "SHELL:$<IF:$<PLATFORM_ID:Darwin>,LINKER:-undefined$<COMMA>dynamic_lookup,LINKER:--unresolved-symbols=ignore-all>"
      )
  endif()
endmacro()

function(cet_query_system)
  # This macro is useful if you need to check a variable
  # http://cmake.org/Wiki/CMake_Useful_Variables#Compilers_and_Tools also see
  # http://cmake.org/Wiki/CMake_Useful_Variables/Logging_Useful_Variables
  message(STATUS "cet_query_system: begin system report")
  if(NOT ARGN)
    set(ARGN
        BUILD_SHARED_LIBS
        BUILD_TESTING
        CMAKE_BASE_NAME
        CMAKE_BUILD_TYPE
        CMAKE_CONFIGURATION_TYPES
        CMAKE_CXX_COMPILER
        CMAKE_CXX_COMPILER_ID
        CMAKE_CXX_COMPILER_VERSION
        CMAKE_CXX_EXTENSIONS
        CMAKE_CXX_STANDARD
        CMAKE_CXX_STANDARD_REQUIRED
        CMAKE_C_COMPILER
        CMAKE_C_COMPILER_ID
        CMAKE_C_COMPILER_VERSION
        CMAKE_C_EXTENSIONS
        CMAKE_C_STANDARD
        CMAKE_C_STANDARD_REQUIRED
        CMAKE_Fortran_COMPILER
        CMAKE_Fortran_COMPILER_ID
        CMAKE_Fortran_COMPILER_VERSION
        CMAKE_SYSTEM_NAME
        )
  endif()
  foreach(var IN LISTS ARGN)
    if(DEFINED var)
      message(STATUS "  ${VAR} = ${${VAR}}")
    endif()
  endforeach()
  message(STATUS "cet_query_system: end system report")
endfunction()
