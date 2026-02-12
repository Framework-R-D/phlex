#[=======================================================================[.rst:
FindJsonnet
-----------

.. versionadded:: 3.23.00

The module defines the following variables:

``Jsonnet_EXECUTABLE``
  Location of the jsonnet executable.

``Jsonnet_LIBRARY``
  Location of the libjsonnet library.

``Jsonnet_CXX_LIBRARY``
  Location of the libjsonnet++ library.

``Jsonnet_INCLUDE_DIRS``
  Location of the Jsonnet C/C++ headers.

``Jsonnet_FOUND``
  True if the Jsonnet executable was found.

\... and targets:

``Jsonnet::jsonnet``
  The Jsonnet command-line interpreter.

``Jsonnet::jsonnetfmt``
  The Jsonnet reformatter.

``Jsonnet::libjsonnet``
  The libjsonnet library.

``Jsonnet::libjsonnet++``
  The libjsonnet++ library.

Example usage:

.. code-block:: cmake

   find_package(Jsonnet)
   if(Jsonnet_FOUND)
     message("Jsonnet found: ${Jsonnet_EXECUTABLE}")
   endif()
#]=======================================================================]

# First search the PATH and specific locations.
find_program(
  Jsonnet_EXECUTABLE
  NAMES jsonnet
  DOC "Jsonnet command-line interpreter"
  )
mark_as_advanced(Jsonnet_EXECUTABLE)

if(Jsonnet_EXECUTABLE)
  find_library(
    Jsonnet_LIBRARY
    NAMES jsonnet
    DOC "Jsonnet library"
    )
  mark_as_advanced(Jsonnet_LIBRARY)
  find_library(
    Jsonnet_CXX_LIBRARY
    NAMES jsonnet++
    DOC "Jsonnet C++ library"
    )
  mark_as_advanced(Jsonnet_CXX_LIBRARY)
  if(Jsonnet_LIBRARY OR Jsonnet_CXX_LIBRARY)
    find_path(
      Jsonnet_INCLUDE_DIRS
      NAMES libjsonnet.h libjsonnet++.h libjsonnet_fmt.h
      DOC "Jsonnet header includes"
      )
    mark_as_advanced(Jsonnet_INCLUDE_DIRS)
  endif()

  # Avoid querying the version if we've already done that this run. For projects
  # that use things like ExternalProject or FetchContent heavily, this saving
  # can be measurable on some platforms.
  #
  # This is an internal property, projects must not try to use it. We don't want
  # this stored in the cache because it might still change between CMake runs,
  # but it shouldn't change during a run for a given jsonnet executable
  # location.
  set(__doJsonnetVersionCheck TRUE)
  get_property(
    __jsonnetVersionProp GLOBAL
    PROPERTY _CETMODULES_FindJsonnet_Jsonnet_EXECUTABLE_VERSION
    )
  if(__jsonnetVersionProp)
    list(GET __jsonnetVersionProp 0 __jsonnetExe)
    list(GET __jsonnetVersionProp 1 __jsonnetVersion)
    list(GET __jsonnetVersionProp 2 __jsonnetVersionExtra)
    if(__jsonnetExe STREQUAL Jsonnet_EXECUTABLE AND NOT __jsonnetVersion
                                                    STREQUAL ""
       )
      set(Jsonnet_VERSION_STRING "${__jsonnetVersion}")
      set(Jsonnet_VERSION_EXTRA "${__jsonnetVersionExtra}")
      set(__doJsonnetVersionCheck FALSE)
    endif()
    unset(__jsonnetExe)
    unset(__jsonnetVersion)
    unset(__jsonnetVersionExtra)
  endif()
  unset(__jsonnetVersionProp)

  if(__doJsonnetVersionCheck)
    execute_process(
      COMMAND ${Jsonnet_EXECUTABLE} --version
      COMMAND sed -Ene "1 s&.*v([0-9][0-9.]*)(.*)$&\\1;\\2&p"
      OUTPUT_VARIABLE __jsonnetVersionString
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    list(GET __jsonnetVersionString 0 Jsonnet_VERSION_STRING)
    list(GET __jsonnetVersionString 1 Jsonnet_VERSION_EXTRA)
    set_property(
      GLOBAL
      PROPERTY
        _CETMODULES_FindJsonnet_Jsonnet_EXECUTABLE_VERSION
        "${Jsonnet_EXECUTABLE};${Jsonnet_VERSION_STRING};${Jsonnet_VERSION_EXTRA}"
      )
  endif()
  unset(__doJsonnetVersionCheck)
  unset(__jsonnetVersionString)

  get_property(_findjsonnet_role GLOBAL PROPERTY CMAKE_ROLE)
  if(_findjsonnet_role STREQUAL "PROJECT")
    if(NOT TARGET Jsonnet::jsonnet)
      add_executable(Jsonnet::jsonnet IMPORTED)
      set_property(
        TARGET Jsonnet::jsonnet PROPERTY IMPORTED_LOCATION
                                         "${Jsonnet_EXECUTABLE}"
        )
    endif()
    if(NOT TARGET Jsonnet::jsonnetfmt)
      add_executable(Jsonnet::jsonnetfmt IMPORTED)
      set_property(
        TARGET Jsonnet::jsonnetfmt PROPERTY IMPORTED_LOCATION
                                            "${Jsonnet_EXECUTABLE}fmt"
        )
    endif()
    if(Jsonnet_LIBRARY AND NOT TARGET Jsonnet::libjsonnet)
      add_library(Jsonnet::libjsonnet UNKNOWN IMPORTED)
      set_target_properties(
        Jsonnet::libjsonnet PROPERTIES IMPORTED_LOCATION ${Jsonnet_LIBRARY}
        )
      if(Jsonnet_INCLUDE_DIRS)
        set_target_properties(
          Jsonnet::libjsonnet PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                         "${Jsonnet_INCLUDE_DIRS}"
          )
      endif()
    endif()
    if(Jsonnet_CXX_LIBRARY AND NOT TARGET Jsonnet::libjsonnet++)
      add_library(Jsonnet::libjsonnet++ UNKNOWN IMPORTED)
      set_target_properties(
        Jsonnet::libjsonnet++ PROPERTIES IMPORTED_LOCATION
                                         ${Jsonnet_CXX_LIBRARY}
        )
      if(Jsonnet_INCLUDE_DIRS)
        set_target_properties(
          Jsonnet::libjsonnet++ PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                                           "${Jsonnet_INCLUDE_DIRS}"
          )
      endif()
    endif()
  endif()
  unset(_findjsonnet_role)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Jsonnet
  REQUIRED_VARS Jsonnet_EXECUTABLE
  VERSION_VAR Jsonnet_VERSION_STRING
  )
