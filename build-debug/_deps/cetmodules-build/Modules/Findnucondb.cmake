#[================================================================[.rst:
Findnucondb
-----------

Finds nucondb library and headers

Imported Targets
^^^^^^^^^^^^^^^^

This module provides the following imported targets, if found:

``nucondb::nucondb``
  The nucondb library


#]================================================================]
# headers
find_file(
  _cet_nucondb_h
  NAMES nucondb.h
  HINTS ENV NUCONDB_FQ_DIR
  PATH_SUFFIXES include
  )
if(_cet_nucondb_h)
  get_filename_component(_cet_nucondb_include_dir "${_cet_nucondb_h}" PATH)
  if(_cet_nucondb_include_dir STREQUAL "/")
    unset(_cet_nucondb_include_dir)
  endif()
endif()
if(EXISTS "${_cet_nucondb_include_dir}")
  set(nucondb_FOUND TRUE)
  get_filename_component(_cet_nucondb_dir "${_cet_nucondb_include_dir}" PATH)
  if(_cet_nucondb_dir STREQUAL "/")
    unset(_cet_nucondb_dir)
  endif()
  set(nucondb_INCLUDE_DIRS "${_cet_nucondb_include_dir}")
  set(nucondb_LIBRARY_DIR "${_cet_nucondb_dir}/lib")
endif()
if(nucondb_FOUND)
  find_library(
    nucondb_LIBRARY
    NAMES nucondb
    PATHS ${nucondb_LIBRARY_DIR}
    )
  if(NOT TARGET nucondb::nucondb)
    add_library(nucondb::nucondb SHARED IMPORTED)
    set_target_properties(
      nucondb::nucondb
      PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${nucondb_INCLUDE_DIRS}"
                 IMPORTED_LOCATION "${nucondb_LIBRARY}"
      )
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  nucondb REQUIRED_VARS nucondb_FOUND nucondb_INCLUDE_DIRS nucondb_LIBRARY
  )

unset(_cet_nucondb_FIND_REQUIRED)
unset(_cet_nucondb_dir)
unset(_cet_nucondb_include_dir)
unset(_cet_nucondb_h CACHE)
