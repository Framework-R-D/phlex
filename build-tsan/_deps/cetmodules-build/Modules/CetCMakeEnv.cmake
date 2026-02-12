#[================================================================[.rst:
CetCMakeEnv
-----------

This module defines the principal initialization function
:command:`cet_cmake_env` defining the cetmodules build environment for
the current project.
#]================================================================]

# Avoid unnecessary repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.19...4.1 FATAL_ERROR)

# Escape characters for literal use in regular expressions.
include(CetRegexEscape)

# Handle setting of versions with a non-numeric component:
include(private/CetHandleExtendedVersion)

# Project variables.
include(ProjectVariable)

# ##############################################################################
# OPTIONS
# ##############################################################################

# What kind of things can we build?
option(BUILD_SHARED_LIBS "Build shared libraries (all projects)." ON)
option(BUILD_STATIC_LIBS "Build static libraries (all projects)." OFF)
option(BUILD_DOCS "Build documentation (all_projects)." OFF)

# RPATH management.
option(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
mark_as_advanced(CMAKE_INSTALL_RPATH_USE_LINK_PATH ON)
# ##############################################################################

# ##############################################################################
# Configure installation subdirectories.
# ##############################################################################

# Default subdirectory for libraries.
if(NOT CMAKE_INSTALL_LIBDIR)
  set(CMAKE_INSTALL_LIBDIR lib) # Don't use lib64 for installation dir.
endif()

# See https://cmake.org/cmake/help/latest/module/GNUInstallDirs.html.
#
# We suppress developer warnings for this `include()` to silence
# complaints from CMake >=4 when no `LANGUAGES` are enabled for this
# project.
set(_cce_suppress_dev_warnings "$CACHE{CMAKE_SUPPRESS_DEVELOP_WARNINGS}")
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE INTERNAL "" FORCE)
include(GNUInstallDirs)
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS "${_cce_suppress_dev_warnings}" CACHE INTERNAL "" FORCE)
unset(_cce_suppress_dev_warnings)
# ##############################################################################

define_property(
  TARGET
  PROPERTY CET_EXEC_LOCATION
  BRIEF_DOCS "Saved location of the executable represented by a target"
  FULL_DOCS "Saved location of the executable represented by a target"
  )

#[================================================================[.rst:
.. command:: cet_cmake_env

   Set up the cetmodules build environment for the current project.

   .. code-block:: cmake

      cet_cmake_env([NO_INSTALL_PKGMETA])

   Options
   ^^^^^^^

   ``NO_INSTALL_PKGMETA``
     Under normal circumstances, :command:`!cet_cmake_env` will call
     :command:`install_pkgmeta` to automatically find ``LICENSE`` and
     ``README`` files and install them. Specify ``NO_INSTALL_PKGMETA``
     if you wish to call :command:`install_pkgmeta` yourself (or not
     at all).

   Notes
   ^^^^^

   .. note::

      Prior to calling :command:`cet_cmake_env`:

      * The current project must have been initialized via
        :command:`project() <cmake-ref-current:command:project>`

      * Any initial or override values for
        :manual:`project variables <cetmodules-project-variables(7)>` should be set.

   .. seealso:: :command:`cet_finalize`

   Variables affecting behavior
   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^

   * :variable:`BUILD_SHARED_LIBS <cmake-ref-current:variable:BUILD_SHARED_LIBS>`
   * :variable:`BUILD_STATIC_LIBS`
   * :variable:`CMAKE_INSTALL_RPATH_USE_LINK_PATH <cmake-ref-current:variable:CMAKE_INSTALL_RPATH_USE_LINK_PATH>`

#]================================================================]

macro(cet_cmake_env)
  # project() must have been called first.
  if(NOT CMAKE_PROJECT_NAME)
    message(
      FATAL_ERROR
        "CMake project() command must have been invoked prior to cet_cmake_env()."
        "\nIt must be invoked from the project's top level CMakeLists.txt, not in an included .cmake file."
      )
  endif()

  set(CETMODULES_CURRENT_PROJECT_NAME ${PROJECT_NAME})

  # ############################################################################
  # Policy settings
  # ############################################################################

  # Required to ensure correct installation location with
  # cetbuildtools-compatible installations.
  #
  # * Since these cmake_policy() commands are in the *implementation* of a
  #   macro, they are effective within the entire policy stack active at the
  #   time of invocation.
  #
  # See https://cmake.org/cmake/help/latest/policy/CMP0082.html
  cmake_policy(SET CMP0082 NEW)

  # ############################################################################
  # Remove unwanted information from any previous run.
  # ############################################################################
  _clean_internal_cache_entries()

  # ############################################################################
  # Project variables - see ProjectVariable.cmake. See especially the
  # explanation of initialization and default semantics.
  # ############################################################################

  # Enable the config file generator to detect whether we're being built and
  # imported in the same run. Note that this variable is *not* exported to
  # external dependents.
  project_variable(
    IN_TREE
    TRUE
    TYPE
    BOOL
    DOCSTRING
    "Signifies whether ${CETMODULES_CURRENT_PROJECT_NAME} is currently being built"
    )

  # Defined first, as PATH_FRAGMENT and FILEPATH_FRAGMENT project variables take
  # this into account.
  project_variable(EXEC_PREFIX TYPE STRING)

  # Enable projects to specify an extended version with a non-numeric trailing
  # component (e.g. -rc1, or -alpha, or -p03), and deal with it in the right
  # places.
  project_variable(
    CMAKE_PROJECT_VERSION_STRING
    ${PROJECT_VERSION}
    TYPE
    STRING
    DOCSTRING
    "Extended project version, with optional non-numeric trailing \
component(M[.m[.p[.t]]][-X])\
"
    )

  # Ensure we have version settings.
  _cet_handle_extended_version()

  # Should we generate plugin registration libraries of type MODULE_LIBRARY
  # (recommended), or SHARED_LIBRARY (prone to ODR violations)?
  project_variable(
    MODULE_PLUGINS
    TRUE
    TYPE
    BOOL
    DOCSTRING
    "\
Whether plugin registration libraries for project \
${CETMODULES_CURRENT_PROJECT_NAME} should be of type MODULE_LIBRARY (TRUE) or \
SHARED_LIBRARY (FALSE)"
    )

  # Determine whether we are attempting to support extended version semantics
  # (non-numeric version components).
  if("${PROJECT_VERSION_EXTRA}" STREQUAL "")
    set(_cce_ext_v_def FALSE)
  else()
    set(_cce_ext_v_def TRUE)
  endif()

  project_variable(
    EXTENDED_VERSION_SEMANTICS
    ${_cce_ext_v_def}
    TYPE
    BOOL
    DOCSTRING
    "Use extended version semantics permitting a non-numeric trailing \
component, with sensible comparison semantics for alpha, rc and other \
recognized version formats\
"
    )

  if(_cce_ext_v_def
     AND NOT ${CETMODULES_CURRENT_PROJECT_NAME}_EXTENDED_VERSION_SEMANTICS
     )
    message(
      SEND_ERROR
        "Extended semantics for CMAKE_PROJECT_VERSION (${PROJECT_VERSION}) for project ${CETMODULES_CURRENT_PROJECT_NAME} prohibited by ${CETMODULES_CURRENT_PROJECT_NAME}_EXTENDED_VERSION_SEMANTICS"
      )
  endif()
  unset(_cce_ext_v_def)
  # ############################################################################

  # Avoid confusion with nested subprojects.
  foreach(
    _cce_v IN
    ITEMS BINARY_DIR
          DESCRIPTION
          HOMEPAGE_URL
          SOURCE_DIR
          VERSION
          VERSION_MAJOR
          VERSION_MINOR
          VERSION_PATCH
          VERSION_TWEAK
          VERSION_EXTRA
          VERSION_EXTRA_TYPE
          VERSION_EXTRA_TEXT
          VERSION_EXTRA_NUM
    )
    set(CETMODULES_CURRENT_PROJECT_${_cce_v} ${PROJECT_${_cce_v}})
  endforeach()
  unset(_cce_v)

  # Check for obsolete arguments.
  if(NOT "${ARGV}" STREQUAL "")
    warn_deprecated(
      "cet_cmake_env(${ARGV})" " - remove unneeded legacy arguments ${ARGV}"
      )
  endif()

  # Disable package registry use as confusing and not best practice.
  set(CMAKE_EXPORT_PACKAGE_REGISTRY FALSE)
  set(CMAKE_EXPORT_NO_PACKAGE_REGISTRY TRUE)
  set(CMAKE_FIND_USE_PACKAGE_REGISTRY FALSE)
  set(CMAKE_FIND_PACKAGE_NO_SYSTEM_PACKAGE_REGISTRY TRUE)

  # Ask CMake to exit on error at install time if it is asked to install files
  # in an absolute location.
  set(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION TRUE)

  project_variable(
    NAMESPACE
    "${CETMODULES_CURRENT_PROJECT_NAME}"
    TYPE
    STRING
    CONFIG
    OMIT_IF_NULL
    DOCSTRING
    "Top-level prefix for targets and aliases when imported"
    )
  project_variable(
    INCLUDE_DIR
    "${CMAKE_INSTALL_INCLUDEDIR}"
    CONFIG
    OMIT_IF_EMPTY
    OMIT_IF_MISSING
    OMIT_IF_NULL
    DOCSTRING
    "Directory below prefix to install headers"
    )
  project_variable(
    LIBRARY_DIR
    "${CMAKE_INSTALL_LIBDIR}"
    CONFIG
    OMIT_IF_EMPTY
    OMIT_IF_MISSING
    OMIT_IF_NULL
    DOCSTRING
    "Directory below prefix to install libraries"
    )
  project_variable(
    BIN_DIR
    "${CMAKE_INSTALL_BINDIR}"
    CONFIG
    OMIT_IF_EMPTY
    OMIT_IF_MISSING
    OMIT_IF_NULL
    DOCSTRING
    "Directory below prefix to install executables"
    )
  project_variable(
    SCRIPTS_DIR
    ${${CETMODULES_CURRENT_PROJECT_NAME}_BIN_DIR}
    NO_WARN_REDUNDANT
    BACKUP_DEFAULT
    scripts
    OMIT_IF_EMPTY
    OMIT_IF_MISSING
    OMIT_IF_NULL
    DOCSTRING
    "Directory below prefix to install scripts"
    )
  project_variable(
    TEST_DIR
    "test"
    NO_WARN_REDUNDANT
    OMIT_IF_EMPTY
    OMIT_IF_MISSING
    OMIT_IF_NULL
    DOCSTRING
    "Directory below prefix to install tests"
    )
  project_variable(
    DATA_ROOT_DIR
    ${CMAKE_INSTALL_DATAROOTDIR}
    NO_WARN_REDUNDANT
    OMIT_IF_EMPTY
    OMIT_IF_MISSING
    OMIT_IF_NULL
    DOCSTRING
    "Architecture-independent data directory"
    )

  # ############################################################################
  # Default locations for libraries and executables.
  # ############################################################################

  # Override on a per-target, per config or per-scope basis if necessary (should
  # be rare).
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
      ${PROJECT_BINARY_DIR}/${${CETMODULES_CURRENT_PROJECT_NAME}_LIBRARY_DIR}
      )
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
      ${PROJECT_BINARY_DIR}/${${CETMODULES_CURRENT_PROJECT_NAME}_LIBRARY_DIR}
      )
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
      ${PROJECT_BINARY_DIR}/${${CETMODULES_CURRENT_PROJECT_NAME}_BIN_DIR}
      )

  # ############################################################################
  # Avoid warnings about some variables that are likely to have been set on the
  # command line but that may not be used.
  _use_maybe_unused()
endmacro(cet_cmake_env)

function(_clean_internal_cache_entries)
  get_property(
    cache_vars
    DIRECTORY
    PROPERTY CACHE_VARIABLES
    )
  cet_regex_escape("${CETMODULES_CURRENT_PROJECT_NAME}" e_proj)
  list(FILTER cache_vars INCLUDE REGEX
       "^_?CETMODULES(_[^_]+)*_PROJECT_${e_proj}$"
       )
  foreach(entry IN LISTS cache_vars)
    get_property(
      type
      CACHE "${entry}"
      PROPERTY TYPE
      )
    if(type STREQUAL "INTERNAL")
      unset("${entry}" CACHE)
    endif()
  endforeach()
endfunction()

function(_use_maybe_unused)
  get_property(
    cache_vars
    DIRECTORY
    PROPERTY CACHE_VARIABLES
    )
  list(FILTER cache_vars INCLUDE REGEX "_INIT$")
  foreach(
    var IN
    LISTS cache_vars
    ITEMS CMAKE_WARN_DEPRECATED
    )
    if(${var})

    endif()
  endforeach()
endfunction()
