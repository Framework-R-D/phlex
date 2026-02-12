#[================================================================[.rst:
X
-
#]================================================================]

# find_package(<find_package-args>... [PUBLIC|PRIVATE] [REQUIRED_BY
# <components>...])
#
# External dependencies specified using find_package() will be automatically
# collated and added to ${CETMODULES_CURRENT_PROJECT_NAME}Config.cmake as
# approriate (see OPTIONS).
#
# ##############################################################################
# OPTIONS
#
# BUILD_ONLY PRIVATE INTERFACE PUBLIC
#
# If PUBLIC or INTERFACE is specified (or we are maintaining compatibility with
# the older cetbduiltools), an appropriate find_dependency() call will be added
# to this package's Config.cmake file to ensure that the required package will
# be found when necessary for dependent packages; BUILD_ONLY or PRIVATE will not
# add such a call.
#
# REQUIRED_BY <components>
#
# If this dependency is not universally required by all the components your
# package provides, this option will ensure that it will only be loaded when a
# dependent package requests the relevant component(s).
#
# ##############################################################################
# NOTES
#
# * Minimize unwanted dependencies downstream by using PUBLIC or
#   PRIVATE/BUILD_ONLY as necessary to match their use in cet_make(),
#   cet_make_library(), cet_make_exec() and their CMake equivalents,
#   add_library(), and add_executable().
#
# * find_package() will NOT invoke CMake directives with global effect such as
#   include_directories(). Use target_link_libraries() instead with target
#   (package::lib_name) rather than variable (PACKAGE_...) to ensure that all
#   PUBLIC headers associated with a library will be found.
#
# * Works best when combined with appropriate use of PUBLIC, INTERFACE and
#   PRIVATE (or BUILD_ONLY) with cet_make_library() - or alternatively,
#   target_link_libraries() and target_include_directories() - minimizing
#   unwanted transitive dependencies downstream.
#
# * Multiple distinct find_package() directives (perhaps with different
#   requirement levels or component settings) will be propagated for execution
#   in the order they were encountered. find_package() will minimize duplication
#   of effort internally.
#
# * From the point of view of find_package(), INTERFACE AND EXPORT are
#   identical: a find_package() call will be executed either way in order to
#   ensure that targets, etc., are known to CMake at the appropriate time.
# ##############################################################################

# ##############################################################################
# N.B. We override `find_package()` directly rather than using the
# `cmake_language(SET_DEPENDENCY_PROVIDER)` due to the latter's
# restrictions on the version argument and any new flags or options.
# ##############################################################################

# Once only per directory!
include_guard()

cmake_minimum_required(VERSION 3.24...4.1 FATAL_ERROR)

include(CetCMakeUtils)
include(ParseVersionString)
include(private/CetAddTransitiveDependency)

include(CMakeFindDependencyMacro)

# Once only!
include_guard(GLOBAL)

execute_process(
  COMMAND ${CMAKE_COMMAND} --help-command find_package
  COMMAND
    sed -E -n -e
    "/((Basic|Full) Signature( and Module Mode)?|signature is)\$/,/\\)\$/ { s&^[[:space:]]+&&g; s&[[:space:]|]+&\\n&g; s&[^A-Z_\\n]&\\n&g; /^[A-Z_]{2,}(\\n|\$)/ ! D; P; D; }"
  OUTPUT_VARIABLE _cet_fp_keywords
  OUTPUT_STRIP_TRAILING_WHITESPACE COMMAND_ERROR_IS_FATAL ANY
  )
if("${_cet_fp_keywords}" STREQUAL "")
  message(
    FATAL_ERROR
      "\
unable to obtain current list of find_package() keywords from CMake ${CMAKE_VERSION} - \
\"Basic Signature\" heading change?\
"
    )
endif()
string(REPLACE "\n" ";" _cet_fp_keywords ${_cet_fp_keywords})
list(REMOVE_DUPLICATES _cet_fp_keywords)
set(_cet_fp_keywords
    ${_cet_fp_keywords}
    CACHE INTERNAL "List of original find_package() keywords"
    )
set(_cet_fp_new_flags
    BUILD_ONLY EXPORT INTERFACE NOP PRIVATE PUBLIC
    CACHE INTERNAL "List of new find_package() flags"
    )
set(_cet_fp_new_options
    REQUIRED_BY
    CACHE INTERNAL "List of new find_package() options"
    )
set(_cet_fp_new_keywords
    ${_cet_fp_new_flags} ${cet_fp_new_options}
    CACHE INTERNAL "All new find_package() keywords"
    )
set(_cet_fp_all_keywords
    ${_cet_fp_keywords} ${_cet_fp_new_keywords}
    CACHE INTERNAL "All find_package() keywords"
    )

if (COMMAND _find_package)
  message(FATAL_ERROR "find_package() has already been overridden: cetmodules cannot function")
endif()

option(CET_FIND_QUIETLY "All find_package() calls will be quiet." OFF)

# Intercept calls to find_package() for IN_TREE packages and make them do the
# right thing.
macro(find_package PKG)
  # Due to the high likelihood that find_package() calls will be nested, we need
  # to be extremely careful to reset variables to avoid hysteresis.
  _cet_fp_reset_variables()
  _cet_fp_parse_args(${ARGV})
  math(EXPR _fp_finding_${PKG} "${_fp_finding_${PKG}} + 1")
  if(_fp_finding_${PKG} EQUAL 1)
    # Handle nested calls (e.g. FindXXXX.cmake -> XXXConfig.cmake...)
    if(_fp_BUILD_ONLY
       OR _fp_PRIVATE
       OR NOT
          (_fp_INTERFACE
           OR _fp_PUBLIC
           OR _fp_EXPORT)
       )
      set(_fp_NO_EXPORT_${PKG} TRUE)
    else()
      unset(_fp_NO_EXPORT_${PKG})
    endif()
    set(_fp_${PKG}_REQUIRED_BY ${_fp_REQUIRED_BY})
    set(_fp_${PKG}_transitive_args ${PKG} ${_fp_minver_${PKG}}
                                   ${_fp_UNPARSED_ARGUMENTS}
        )
  endif()
  if(${PKG}_IN_TREE) # Package we need is being built with us.
    if(NOT CMAKE_DISABLE_FIND_PACKAGE_${PKG} OR _fp_UNPARSED_ARGUMENTS MATCHES
                                                "(^|;)REQUIRED(;|$)"
       )
      string(TOUPPER "${PKG}" _fp_PKG_UC)
      # May be modified by transitive dependency searches.
      set(${PKG}_FOUND TRUE)
      # Add any CMake module directories to CMAKE_MODULE_PATH.
      if(CETMODULES_CMAKE_MODULES_DIRECTORIES_PROJECT_${PKG})
        list(
          TRANSFORM CETMODULES_CMAKE_MODULES_DIRECTORIES_PROJECT_${PKG}
          PREPEND "${${PKG}_SOURCE_DIR}/"
          REGEX "^[^/]+" OUTPUT_VARIABLE _fp_module_path_source
          )
        list(
          TRANSFORM CETMODULES_CMAKE_MODULES_DIRECTORIES_PROJECT_${PKG}
          PREPEND "${${PKG}_BINARY_DIR}/"
          REGEX "^[^/]+" OUTPUT_VARIABLE _fp_module_path_binary
          )
        list(PREPEND CMAKE_MODULE_PATH ${_fp_module_path_source}
             ${_fp_module_path_binary}
             )
        unset(_fp_module_path_source)
        unset(_fp_module_path_binary)
      endif()
      # Load transitive dependencies.
      if(CETMODULES_TRANSITIVE_DEPS_PROJECT_${PKG})
        # Save the current value of CMAKE_FIND_PACKAGE_NAME.
        set(_fp_CMAKE_FIND_PACKAGE_NAME_${PKG} ${CMAKE_FIND_PACKAGE_NAME})
        # Update CMAKE_FIND_PACKAGE_NAME to keep the books straight for
        # find_dependency().
        set(CMAKE_FIND_PACKAGE_NAME ${PKG})
        foreach(_cet_dep IN LISTS CETMODULES_TRANSITIVE_DEPS_PROJECT_${PKG})
          cmake_language(EVAL CODE "${_cet_dep}")
          if(NOT ${PKG}_FOUND)
            break()
          endif()
        endforeach()
        # Restore CMAKE_FIND_PACKAGE_NAME.
        set(CMAKE_FIND_PACKAGE_NAME ${_fp_CMAKE_FIND_PACKAGE_NAME_${PKG}})
      endif()
      if(${PKG}_FOUND)
        if(NOT ("${${PKG}_CMAKE_PROJECT_VERSION_STRING}" STREQUAL ""
                OR ${PKG}_CMAKE_PROJECT_VERSION_STRING STREQUAL PROJECT_VERSION
               )
           )
          set(${PKG}_VERSION ${${PKG}_CMAKE_PROJECT_VERSION_STRING})
        endif()
        set(${PKG}_DIR ${${PKG}_BINARY_DIR})
      endif()
    endif()
    if(NOT "${PKG}" STREQUAL CETMODULES_CURRENT_PROJECT_NAME)
      # Localize all project variables for PKG in this directory scope
      cet_localize_pv(${PKG} ALL)
    endif()
    # Record that we "found" this package.
    set(${_fp_PKG_UC}_FOUND ${PKG}_FOUND)
    unset(_fp_PKG_UC)
  else()
    _cet_fp_check_find_package_needed(${PKG} _cet_find_package_needed)
    if(_cet_find_package_needed)
      # Global quiet setting.
      if(CET_FIND_QUIETLY)
        set(_fp_QUIET QUIET)
      else()
        set(_fp_QUIET)
      endif()
      # Underlying built-in find_package() call.
      _find_package(
        ${PKG} ${_fp_minver_${PKG}} ${_fp_UNPARSED_ARGUMENTS}
        ${_fp_QUIET}
        )
      if(DEFINED CACHE{${PKG}_DIR})
        mark_as_advanced(${PKG}_DIR) # Generally don't need to configure this.
      endif()
      # Package-specific fixup if necessary.
      if(COMMAND _cet_${PKG}_post_find_package)
        cmake_language(CALL _cet_${PKG}_post_find_package)
      endif()
      # Cleanup.
    endif()
    unset(_cet_find_package_needed)
  endif()
  if(_fp_finding_${PKG} EQUAL 1)
    # Determine whether we need to add ${PKG} as a transitive dependency of the
    # package currently being built.
    if(${PKG}_FOUND AND NOT "${_fp_${PKG}_transitive_args}" STREQUAL "")
      foreach(_fp_component IN LISTS _fp_${PKG}_REQUIRED_BY)
        _cet_add_transitive_dependency(
          find_package COMPONENT ${_fp_component} ${_fp_${PKG}_transitive_args}
          )
      endforeach()
      _cet_add_transitive_dependency(find_package ${_fp_${PKG}_transitive_args})
    endif()
    unset(_fp_${PKG}_REQUIRED_BY)
    unset(_fp_${PKG}_transitive_args)
    unset(_fp_finding_${PKG})
    unset(_fp_NO_EXPORT_${PKG})
  else()
    math(EXPR _fp_finding_${PKG} "${_fp_finding_${PKG}} - 1")
  endif()
  unset(_fp_minver_${PKG})
  unset(_fp_COMPONENTS)
  unset(_fp_OPTIONAL_COMPONENTS)
  unset(${PKG}_FIND_VERSION_MIN_EXTRA)
  unset(${PKG}_FIND_VERSION_MAX_EXTRA)
endmacro()

macro(_cet_fp_reset_variables)
  foreach(
    _fp_keyword IN
    LISTS _cet_fp_new_keywords
    ITEMS COMPONENTS OPTIONAL_COMPONENTS REQUIRED_BY UNPARSED_ARGUMENTS
    )
    unset(_fp_have_${_fp_keyword})
    unset(_fp_${_fp_keyword})
  endforeach()
  unset(_fp_keyword)
endmacro()

function(_cet_fp_check_find_package_needed PKG RESULT_VAR)
  set(${RESULT_VAR}
      TRUE
      PARENT_SCOPE
      )
  unset(${PKG}_FOUND CACHE) # So we will (e.g.) redefine targets if necessary.
  if(NOT ${PKG}_FOUND)
    return()
  endif()
  foreach(component IN LISTS _fp_COMPONENTS _fp_OPTIONAL_COMPONENTS
                             ${PKG}_FIND_COMPONENTS
          )
    if(NOT ${PKG}_${component}_FOUND)
      return()
    endif()
  endforeach()
  set(${RESULT_VAR}
      FALSE
      PARENT_SCOPE
      )
endfunction()

function(_cet_fp_parse_args PKG)
  set(_fp_args "${ARGN}")
  if("${PKG}" STREQUAL "" OR "${_fp_args}" STREQUAL "")
    return()
  endif()
  # ############################################################################
  # Make sure options and arguments not understood by CMake's find_package() are
  # filtered out before it sees them. Also separate COMPONENTS and
  # OPTIONAL_COMPONENTS, adding them back in later.
  foreach(_fp_arg IN ITEMS REQUIRED_BY COMPONENTS OPTIONAL_COMPONENTS)
    unset(_fp_${_fp_arg})
    unset(_fp_have_${_fp_arg})
    while("${_fp_arg}" IN_LIST _fp_args)
      set(_fp_have_${_fp_arg} TRUE)
      list(FIND _fp_args "${_fp_arg}" _fp_idx)
      list(REMOVE_AT _fp_args ${_fp_idx})
      list(LENGTH _fp_args _fp_req_len)
      while(_fp_idx LESS _fp_req_len)
        list(GET _fp_args ${_fp_idx} _fp_req_item)
        if(_fp_req_item IN_LIST _cet_fp_all_keywords)
          break()
        endif()
        list(REMOVE_AT _fp_args ${_fp_idx})
        list(APPEND _fp_${_fp_arg} "${_fp_req_item}")
        math(EXPR _fp_req_len "${_fp_req_len} - 1")
      endwhile()
    endwhile()
    set(_fp_${_fp_arg}
        ${_fp_${_fp_arg}}
        PARENT_SCOPE
        )
    set(_fp_have_${_fp_arg}
        ${_fp_have_${_fp_arg}}
        PARENT_SCOPE
        )
  endforeach()
  # Add these arguments back in for find_package() proper after consolidation.
  foreach(_fp_arg COMPONENTS OPTIONAL_COMPONENTS)
    if(_fp_have_${_fp_arg})
      list(APPEND _fp_args ${_fp_arg} ${_fp_${_fp_arg}})
    endif()
  endforeach()
  # Check flags
  foreach(_fp_arg IN LISTS _cet_fp_new_flags)
    list(FIND _fp_args "${_fp_arg}" _fp_idx)
    if(_fp_idx GREATER -1)
      set(_fp_${_fp_arg}
          TRUE
          PARENT_SCOPE
          )
      list(REMOVE_AT _fp_args ${_fp_idx})
    else()
      unset(_fp_${_fp_arg} PARENT_SCOPE)
    endif()
  endforeach()
  unset(_fp_idx)
  unset(_fp_arg)
  # ############################################################################

  # ############################################################################
  # Sanitize the minimum and/or maximum version specification(s) in case someone
  # is using extended version semantics not supported by CMake.
  if("${_fp_args}" STREQUAL "")
    set(_fp_have_first_arg)
  else()
    set(_fp_have_first_arg TRUE)
    list(GET _fp_args 0 _fp_version_arg)
  endif()
  if("${_fp_version_arg}" STREQUAL "" OR _fp_version_arg IN_LIST _cet_fp_keywords)
    unset(_fp_minver_${PKG} PARENT_SCOPE)
  elseif(_fp_version_arg MATCHES "^[0-9.]+$")
    # Standard case, or ${PKG}_FIND_VERSION_(MIN|MAX)_EXTRA were already set (in
    # which case we don't need to deal with them).
    if (_fp_have_first_arg)
      list(POP_FRONT _fp_args _fp_minver_${PKG})
    endif()
    set(_fp_minver_${PKG}
        ${_fp_minver_${PKG}}
        PARENT_SCOPE
        )
  elseif(_fp_version_arg MATCHES "(\\.\\.\\.[^.]*)(\\.\\.\\.[^.]*)")
    # Unable to parse unambiguously.
    message(
      FATAL_ERROR
        "cannot parse ambiguous extended version (range?) ${_fp_version_arg}—use 0 for numeric version placeholders."
      )
  else()
    string(REGEX REPLACE "\\.\\.\\.(.+)$" "" _fp_minver_${PKG}
                         "${_fp_version_arg}"
           )
    if(NOT _fp_minver_${PKG} STREQUAL _fp_version_arg)
      set(_fp_maxver_${PKG} "${CMAKE_MATCH_1}")
    else()
      unset(fp_maxver_${PKG})
    endif()
    if(NOT "${_fp_minver_${PKG}}${_fp_maxver_${PKG}}" STREQUAL ""
       AND "${_fp_minver_${PKG}}" MATCHES
           "^(v?[0-9]+([._][0-9]+([._][0-9]+([._][0-9]+)?)?)?|$)"
       AND "${_fp_maxver_${PKG}}" MATCHES
           "^(v?[0-9]+([._][0-9]+([._][0-9]+([._][0-9]+)?)?)?|$)"
       )
      # ${PKG}_FIND_VERSION_(MIN|MAX)_EXTRA might be set already.
      if("${_fp_minver_${PKG}}" MATCHES "^[0-9.]+$")
        string(JOIN "-" _fp_minver_${PKG} "${_fp_minver_${PKG}}"
               ${${PKG}_FIND_VERSION_MIN_EXTRA}
               )
      endif()
      parse_version_string(
        "${_fp_minver_${PKG}}"
        _fp_minver_${PKG}
        NO_EXTRA
        SEP
        .
        EXTRA_VAR
        ${PKG}_FIND_VERSION_MIN_EXTRA
        )
      set(${PKG}_FIND_VERSION_MIN_EXTRA
          ${${PKG}_FIND_VERSION_MIN_EXTRA}
          PARENT_SCOPE
          )
      if("${_fp_maxver_${PKG}}" MATCHES "^[0-9.]+$")
        string(JOIN "-" _fp_maxver_${PKG} "${_fp_maxver_${PKG}}"
               ${${PKG}_FIND_VERSION_MAX_EXTRA}
               )
      endif()
      parse_version_string(
        "${_fp_maxver_${PKG}}"
        _fp_maxver_${PKG}
        NO_EXTRA
        SEP
        .
        EXTRA_VAR
        ${PKG}_FIND_VERSION_MAX_EXTRA
        )
      set(${PKG}_FIND_VERSION_MAX_EXTRA
          ${${PKG}_FIND_VERSION_MAX_EXTRA}
          PARENT_SCOPE
          )
      # Recombine and push upstream.
      string(JOIN "..." _fp_minver_${PKG} "${_fp_minver_${PKG}}"
             ${_fp_maxver_${PKG}}
             )
      set(_fp_minver_${PKG}
          "${_fp_minver_${PKG}}"
          PARENT_SCOPE
          )
      list(REMOVE_AT _fp_args 0)
    else()
      # Should never get here.
      message(
        FATAL_ERROR "internal error parsing find_package(${PKG} ${_fp_args})"
        )
    endif()
  endif()
  # ############################################################################

  # Propagate remaining arguments upward to pass to find_package() proper.
  set(_fp_UNPARSED_ARGUMENTS
      ${_fp_args}
      PARENT_SCOPE
      )
endfunction()

macro(_cet_ROOT_post_find_package)
  # ROOT doesn't set ROOT_<component>_FOUND according to convention.
  foreach(component IN LISTS _cet_fp_COMPONENTS _cet_fp_OPTIONAL_COMPONENTS
                             ROOT_FIND_COMPONENTS
          )
    if(ROOT_${component}_LIBRARY AND TARGET ROOT::${component})
      set(ROOT_${component}_FOUND TRUE)
    endif()
  endforeach()
endmacro()
