########################################################################
# This version file is capable of comparing versions with a trailing
# non-numeric component (cf
# ${CMAKE_ROOT}/Modules/BasicConfigVersion-AnyNewerVersion.cmake.in).
#
# We set PACKAGE_VERSION_EXACT if the current version and the requested
# version compare equal, and PACKAGE_VERSION_COMPATIBLE if the current
# version either does not compare less than the requested version, or is
# within the requested range if such is specified.
########################################################################
cmake_policy(PUSH)
cmake_minimum_required(VERSION 3.12...3.27 FATAL_ERROR)

set(PACKAGE_VERSION "4.01.01")
set(PACKAGE_VERSION_INFO "4;01;01")

if (PACKAGE_VERSION VERSION_EQUAL 0 AND PACKAGE_VERSION MATCHES "[^0-9._-]")
  # Non-numeric versions are always compatible.
  set(PACKAGE_VERSION_COMPATIBLE TRUE)
else()
  # Make sure we have access to a suitably recent version comparator.
  set(_CET_PARSE_VERSION_STRING_MIN_CETMODULES_VERSION "2.21.00")
  if (NOT (cetmodules_FOUND OR cetmodules_IN_TREE OR PACKAGE_FIND_NAME STREQUAL "cetmodules"))
    find_package(cetmodules ${_CET_PARSE_VERSION_STRING_MIN_CETMODULES_VERSION} QUIET)
  endif()
  if (NOT (cetmodules_FOUND OR cetmodules_IN_TREE) OR
      cetmodules_VERSION VERSION_LESS _CET_PARSE_VERSION_STRING_MIN_CETMODULES_VERSION)
    include(${CMAKE_CURRENT_LIST_DIR}/ParseVersionString.cmake)
  elseif (NOT COMMAND cet_compare_versions)
    include(ParseVersionString)
  endif()
  unset(_CET_PARSE_VERSION_STRING_MIN_CETMODULES_VERSION)

  if ("${PACKAGE_VERSION_INFO}" STREQUAL "")
    # Parse this version into a list of components for re-use.
    parse_version_string("${PACKAGE_VERSION}" PACKAGE_VERSION_INFO)
  endif()

  string(JOIN "-" PACKAGE_FIND_VERSION_FULL ${PACKAGE_FIND_VERSION} ${${PACKAGE_FIND_NAME}_FIND_VERSION_MIN_EXTRA})
  cet_version_cmp(_cet_version_cmp PACKAGE_VERSION_INFO "${PACKAGE_FIND_VERSION_FULL}")
  unset(PACKAGE_FIND_VERSION_FULL)
  if (PACKAGE_FIND_VERSION VERSION_EQUAL 0 AND NOT "${${PACKAGE_FIND_NAME}_FIND_VERSION_MIN_EXTRA}" STREQUAL "")
    set(_cet_fvm_compatible TRUE)
  else()
    set(_cet_fvm_compatible)
  endif()
  if (_cet_version_cmp LESS 0 AND NOT _cet_fvm_compatible)
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
  elseif (PACKAGE_FIND_VERSION_RANGE)
    # Package version must be in the requested version range.
    if (PACKAGE_FIND_VERSION_MAX VERSION_EQUAL 0 AND NOT "${${PACKAGE_FIND_NAME}_FIND_VERSION_MAX_EXTRA}" STREQUAL "")
      set(PACKAGE_VERSION_COMPATIBLE TRUE)
    else()
      string(JOIN "-" PACKAGE_FIND_VERSION_MAX_FULL ${PACKAGE_FIND_VERSION_MAX} ${${PACKAGE_FIND_NAME}_FIND_VERSION_MAX_EXTRA})
      cet_version_cmp(_cet_version_cmp PACKAGE_VERSION_INFO "${PACKAGE_FIND_VERSION_MAX_FULL}")
      unset(PACKAGE_FIND_VERSION_MAX_FULL)
      if (_cet_version_cmp GREATER 0 OR
          (PACKAGE_FIND_VERSION_RANGE_MAX STREQUAL "EXCLUDE" AND _cet_version_cmp EQUAL 0))
        set(PACKAGE_VERSION_COMPATIBLE FALSE)
      else()
        set(PACKAGE_VERSION_COMPATIBLE TRUE)
      endif()
    endif()
  else()
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
    if (_cet_version_cmp EQUAL 0)
      set(PACKAGE_VERSION_EXACT TRUE)
    endif()
  endif()
  unset(_cet_fvm_compatible)
  unset(_cet_version_cmp)
endif()

cmake_policy(POP)

# If the installed project requested no architecture check (or the
# current or installation platform doesn't have CMAKE_SIZEOF_VOID_P set,
# don't check.
if ("FALSE" OR
    "${CMAKE_SIZEOF_VOID_P}" STREQUAL "" OR "" STREQUAL "")
  return()
endif()

# Check that the installed version has the same 32/64bit-ness as the one
# which is currently searching:
if (NOT CMAKE_SIZEOF_VOID_P STREQUAL "")
  math(EXPR _cet_installedBits " * 8")
  set(PACKAGE_VERSION "${PACKAGE_VERSION} (${_cet_installedBits}bit)")
  set(PACKAGE_VERSION_UNSUITABLE TRUE)
  unset(_cet_installedBits)
endif()
unset(PACKAGE_VERSION_INFO)
