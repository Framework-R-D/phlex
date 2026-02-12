#[================================================================[.rst:
Findifbeam
----------

Finds ifbeam library and headers

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``ifbeam::ifbeam``
  The ifbeam library


#]================================================================]
# headers
find_file(
  _cet_ifbeam_h
  NAMES ifbeam.h
  HINTS ENV IFBEAM_FQ_DIR
  PATH_SUFFIXES include
  )
if(_cet_ifbeam_h)
  get_filename_component(_cet_ifbeam_include_dir "${_cet_ifbeam_h}" PATH)
  if(_cet_ifbeam_include_dir STREQUAL "/")
    unset(_cet_ifbeam_include_dir)
  endif()
endif()
if(EXISTS "${_cet_ifbeam_include_dir}")
  set(ifbeam_FOUND TRUE)
  get_filename_component(_cet_ifbeam_dir "${_cet_ifbeam_include_dir}" PATH)
  if(_cet_ifbeam_dir STREQUAL "/")
    unset(_cet_ifbeam_dir)
  endif()
  set(ifbeam_INCLUDE_DIRS "${_cet_ifbeam_include_dir}")
  set(ifbeam_LIBRARY_DIR "${_cet_ifbeam_dir}/lib")
endif()
if(ifbeam_FOUND)
  find_library(
    ifbeam_LIBRARY
    NAMES ifbeam
    PATHS ${ifbeam_LIBRARY_DIR}
    )
  if(NOT TARGET ifbeam::ifbeam)
    add_library(ifbeam::ifbeam SHARED IMPORTED)
    set_target_properties(
      ifbeam::ifbeam
      PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${ifbeam_INCLUDE_DIRS}"
                 IMPORTED_LOCATION "${ifbeam_LIBRARY}"
      )
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  ifbeam REQUIRED_VARS ifbeam_FOUND ifbeam_INCLUDE_DIRS ifbeam_LIBRARY
  )

unset(_cet_ifbeam_FIND_REQUIRED)
unset(_cet_ifbeam_dir)
unset(_cet_ifbeam_include_dir)
unset(_cet_ifbeam_h CACHE)
