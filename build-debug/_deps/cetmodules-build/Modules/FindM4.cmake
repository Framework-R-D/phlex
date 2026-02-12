#[=======================================================================[.rst:
FindM4
-------

.. versionadded:: 3.23.00

The module defines the following variables:

``M4_EXECUTABLE``
  Path to M4 command-line client.
``M4_FOUND``, ``M4_FOUND``
  True if the M4 command-line client was found.
``M4_IS_GNU``
  True if the m4 found is GNU m4.
``M4_POSIX_OPT``
  If applicable, this is set to the option used to require POSIX
  behavior. if ``M4_IS_GNU`` is true, this should be ``-G``.
``M4_VERSION_STRING``
  The version of M4 found.

  The module defines the following ``IMPORTED`` targets (when
  :prop_gbl:`CMAKE_ROLE <cmake-ref-current:prop_gbl:CMAKE_ROLE>` is
  ``PROJECT``):

``M4::M4``
  Executable of the M4 command-line client.

Example usage:

.. code-block:: cmake

   find_package(M4)
   if(M4_FOUND)
     message("M4 found: ${M4_EXECUTABLE}")
   endif()
#]=======================================================================]

# First search the PATH and specific locations.
find_program(
  M4_EXECUTABLE
  NAMES m4
  DOC "M4 macro processor"
  )

mark_as_advanced(M4_EXECUTABLE)

if(M4_EXECUTABLE)
  # Avoid querying the version if we've already done that this run. For projects
  # that use things like ExternalProject or FetchContent heavily, this saving
  # can be measurable on some platforms.
  #
  # This is an internal property, projects must not try to use it. We don't want
  # this stored in the cache because it might still change between CMake runs,
  # but it shouldn't change during a run for a given m4 executable location.
  set(__doM4VersionCheck TRUE)
  get_property(
    __m4VersionProp GLOBAL PROPERTY _CETMODULES_FindM4_M4_EXECUTABLE_VERSION
    )
  if(__m4VersionProp)
    list(GET __m4VersionProp 0 __m4Exe)
    list(GET __m4VersionProp 1 __m4Version)
    if(__m4Exe STREQUAL M4_EXECUTABLE AND NOT __m4Version STREQUAL "")
      set(M4_VERSION_STRING "${__m4Version}")
      set(__doM4VersionCheck FALSE)
    endif()
    unset(__m4Exe)
    unset(__m4Version)
  endif()
  unset(__m4VersionProp)

  if(__doM4VersionCheck)
    execute_process(
      COMMAND ${M4_EXECUTABLE} --version
      COMMAND sed -Ene "1 s&^.*(GNU)?.* ([0-9][0-9.]*).*$&\\1;\\2&p"
      OUTPUT_VARIABLE __m4VersionString
      ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE
      )
    list(GET __m4VersionString 0 __m4GnuString)
    list(GET __m4VersionString 1 M4_VERSION_STRING)
    if(__m4GnuString)
      set(M4_IS_GNU TRUE)
      set(M4_POSIX_OPT "-G")
    else()
      set(M4_IS_GNU FALSE)
      set(M4_POSIX_OPT "")
    endif()
    unset(__m4GnuString)
    set_property(
      GLOBAL PROPERTY _CETMODULES_FindM4_M4_EXECUTABLE_VERSION
                      "${M4_EXECUTABLE};${M4_VERSION_STRING}"
      )
  endif()
  unset(__doM4VersionCheck)
  unset(__m4VersionString)

  get_property(_findm4_role GLOBAL PROPERTY CMAKE_ROLE)
  if(_findm4_role STREQUAL "PROJECT" AND NOT TARGET M4::M4)
    add_executable(M4::M4 IMPORTED)
    set_property(TARGET M4::M4 PROPERTY IMPORTED_LOCATION "${M4_EXECUTABLE}")
  endif()
  unset(_findm4_role)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  M4
  REQUIRED_VARS M4_EXECUTABLE
  VERSION_VAR M4_VERSION_STRING
  )
