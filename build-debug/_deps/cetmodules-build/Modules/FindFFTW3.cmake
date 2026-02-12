#[================================================================[.rst:
FindFFTW3[flq]?
---------------
#]================================================================]
# Mitigate a possible FFTW3 packaging error if built with autotools instead of
# CMake.

# Attempt to load the normal way without fatal error on failure.
set(_cet_findfftw3_required ${${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED})
set(${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED)
set(_cet_findfftw3_quietly ${${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY})
set(${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY TRUE)
find_package(${CMAKE_FIND_PACKAGE_NAME} CONFIG QUIET)
set(${CMAKE_FIND_PACKAGE_NAME}_FIND_QUIETLY ${_cet_findfftw3_quietly})
set(${CMAKE_FIND_PACKAGE_NAME}_FIND_REQUIRED ${_cet_findfftw3_required})
unset(_cet_findfftw3_quietly)
unset(_cet_findfftw3_required)

# Alternative attempt to find FFTW3* using pkg-config.
if(NOT (${CMAKE_FIND_PACKAGE_NAME}_FOUND AND TARGET
                                             FFTW3::${CMAKE_FIND_PACKAGE_NAME})
   )
  unset(${CMAKE_FIND_PACKAGE_NAME}_FOUND)
  unset(${CMAKE_FIND_PACKAGE_NAME}_FOUND CACHE)
  include(CetFindPkgConfigPackage)
  string(TOLOWER "${CMAKE_FIND_PACKAGE_NAME}" _cet_findfftw3_package_name)
  cet_find_pkg_config_package(NAMESPACE FFTW3 ${_cet_findfftw3_package_name})
  unset(_cet_findfftw3_package_name)
endif()
