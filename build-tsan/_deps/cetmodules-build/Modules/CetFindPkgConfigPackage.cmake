#[================================================================[.rst:
CetFindPkgConfigPackage
-----------------------

Define the function :command:`cet_find_pkg_config_package` for use in
:external+cmake-ref-current:ref:`CMake Find modules <find modules>`.

#]================================================================]
include_guard()

cmake_minimum_required(VERSION 3.5...4.1 FATAL_ERROR)

include(FindPackageHandleStandardArgs)

#[================================================================[.rst:
.. command:: cet_find_pkg_config_package

   Find a package using :manpage:`pkg-config(1)`.

   .. rst-class:: text-start

   .. seealso::

      :command:`pkg_search_module() <cmake-ref-current:command:pkg_search_module>`,
      :command:`find_package_handle_standard_args() <cmake-ref-current:command:find_package_handle_standard_args>`,
      :command:`find_package() <cmake-ref-current:command:find_package>`.

   .. code-block:: cmake

      cet_find_pkg_config_package([NAMESPACE <ns>] <module> ...)

   Options
   ^^^^^^^

   ``NAMESPACE <ns>``
     Targets for found modules will be prefaced by ``<ns>:``

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<module>`` ...
     The ``pkg-config`` module(s) to find.

   Details
   ^^^^^^^

   ``cet_find_pkg_config_package()`` is intended for use only in the
   context of a :command:`find_package()
   <cmake-ref-current:command:find_package>` call, and as such should be
   invoked within a :external+cmake-ref-current:ref:`CMake Find module
   <find modules>`. ``cet_find_pkg_config_package()`` honors variables
   defined by and for the duration of a :command:`find_package()
   <cmake-ref-current:command:find_package>` call such as
   ``CMAKE_FIND_PACKAGE_NAME``, or ``<package-name>_FIND_VERSION``.

#]================================================================]

macro(cet_find_pkg_config_package)
  cmake_parse_arguments(_cet_find_pkg_config_pkg "" "NAMESPACE" "" ${ARGN})
  if(NOT (_cet_find_pkg_config_package_NAMESPACE
          OR _cet_find_pkg_config_pkg_KEYWORDS_MISSING_VALUES MATCHES
             "(^|;)NAMESPACE(;|$)")
     )
    set(_cet_find_pkg_config_package_NAMESPACE ${CMAKE_FIND_PACKAGE_NAME})
  endif()
  if(NOT "${_cet_find_pkg_config_package_NAMESPACE}" STREQUAL "")
    string(REGEX REPLACE "::$" "" _cet_find_pkg_config_package_NAMESPACE
                         "${_cet_find_pkg_config_package_NAMESPACE}"
           )
    string(APPEND _cet_find_pkg_config_package_NAMESPACE "::")
  endif()
  if(${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY)
    set(_cet_find_pkg_config_package_quiet QUIET)
  else()
    set(_cet_find_pkg_config_package_quiet)
  endif()
  if(${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED)
    set(_cet_find_pkg_config_package_required REQUIRED)
  else()
    set(_cet_find_pkg_config_package_required)
  endif()
  set(_cet_find_pkg_config_pkg_prefix ${CMAKE_FIND_PACKAGE_NAME})
  find_package(PkgConfig REQUIRED QUIET)
  set(CMAKE_FIND_PACKAGE_NAME ${_cet_find_pkg_config_pkg_prefix})
  unset(_cet_find_pkg_config_pkg_prefix)
  set(_cet_find_pkg_config_package_vexpr)
  if(${CMAKE_FIND_PACKAGE_NAME}_FIND_VERSION_RANGE)
    if(${CMAKE_FIND_PACKAGE_NAME}_FIND_VERSION_RANGE_MIN STREQUAL "INCLUDE")
      set(_cet_find_pkg_config_package_vexpr
          "${_cet_find_pk_config_package_vexpr}<=>=${${CMAKE_FIND_PACKAGE_NAME}_FIND_VERSION_MIN}"
          )
    else()
      set(_cet_find_pkg_config_package_vexpr
          "${_cet_find_pk_config_package_vexpr}<=>${${CMAKE_FIND_PACKAGE_NAME}_FIND_VERSION_MIN"
          )
    endif()
    if(${CMAKE_FIND_PACKAGE_NAME}_FIND_VERSION_RANGE_MAX STREQUAL "INCLUDE")
      set(_cet_find_pkg_config_package_vexpr
          "${_cet_find_pk_config_package_vexpr}<=${${CMAKE_FIND_PACKAGE_NAME}_FIND_VERSION_MAX}"
          )
    else()
      set(_cet_find_pkg_config_package_vexpr
          "${_cet_find_pk_config_package_vexpr}<=<${${CMAKE_FIND_PACKAGE_NAME}_FIND_VERSION_MAX}"
          )
    endif()
  elseif(${CMAKE_FIND_PACKAGE_NAME}_FIND_VERSION)
    if(${CMAKE_FIND_PACKAGE_NAME}_FIND_EXACT)
      set(_cet_find_pkg_config_package_vexpr
          "${_cet_find_pk_config_package_vexpr}<==${${CMAKE_FIND_PACKAGE_NAME}_FIND_VERSION}"
          )
    else()
      set(_cet_find_pkg_config_package_vexpr
          "${_cet_find_pk_config_package_vexpr}<=>=${${CMAKE_FIND_PACKAGE_NAME}_FIND_VERSION}"
          )
    endif()
  endif()
  set(_cet_find_pkg_config_package_modules)
  foreach(_cet_find_pkg_config_package_module
          "${_cet_find_pkg_config_pkg_UNPARSED_ARGUMENTS}"
          )
    set(_cet_find_pkg_config_package_modules
        "${_cet_find_pkg_config_package_modules}"
        "${_cet_find_pkg_config_package_module}${_cet_find_pkg_config_package_vexpr}"
        )
  endforeach()
  pkg_search_module(
    ${CMAKE_FIND_PACKAGE_NAME}
    ${_cet_find_pkg_config_package_quiet}
    ${_cet_find_pkg_config_package_required}
    NO_CMAKE_PATH
    NO_CMAKE_ENVIRONMENT_PATH
    IMPORTED_TARGET
    ${_cet_find_pkg_config_package_modules}
    )
  if(${CMAKE_FIND_PACKAGE_NAME}_FOUND
     AND NOT TARGET
         ${_cet_find_pkg_config_package_NAMESPACE}${CMAKE_FIND_PACKAGE_NAME}
     )
    add_library(
      ${_cet_find_pkg_config_package_NAMESPACE}${CMAKE_FIND_PACKAGE_NAME} ALIAS
      PkgConfig::${CMAKE_FIND_PACKAGE_NAME}
      )
  endif()

  find_package_handle_standard_args(
    ${CMAKE_FIND_PACKAGE_NAME}
    VERSION_VAR ${CMAKE_FIND_PACKAGE_NAME}_VERSION
    REQUIRED_VARS ${CMAKE_FIND_PACKAGE_NAME}_PREFIX
    )

  unset(_cet_find_pkg_config_package_KEYWORDS_MISSING_VALUES)
  unset(_cet_find_pkg_config_package_NAMESPACE)
  unset(_cet_find_pkg_config_package_UNPARSED_ARGUMENTS)
  unset(_cet_find_pkg_config_package_module)
  unset(_cet_find_pkg_config_package_modules)
  unset(_cet_find_pkg_config_package_quiet)
  unset(_cet_find_pkg_config_package_required)
  unset(_cet_find_pkg_config_package_vexpr)
endmacro()
