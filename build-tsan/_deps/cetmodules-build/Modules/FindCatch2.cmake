#[================================================================[.rst:
FindCatch2
----------

Find module for Catch2 handling older versions where a CMake config file
is not present.

Imported Targets
^^^^^^^^^^^^^^^^

This module ensures the existence of the ``IMPORTED`` target
``Catch2::Catch2`` if the package is found.

Result Variables
^^^^^^^^^^^^^^^^

.. hlist::
   :columns: 1

   * ``Catch2_FOUND``

#]================================================================]
if(NOT Catch2_FOUND)
  if(Catch2_FIND_REQUIRED)
    set(_cet_Catch2_FIND_REQUIRED ${Catch2_FIND_REQUIRED})
    unset(Catch2_FIND_REQUIRED)
  else()
    unset(_cet_Catch2_FIND_REQUIRED)
  endif()
  find_package(Catch2 CONFIG QUIET)
  if(_cet_Catch2_FIND_REQUIRED)
    set(Catch2_FIND_REQUIRED ${_cet_Catch2_FIND_REQUIRED})
    unset(_cet_Catch2_FIND_REQUIRED)
  endif()
  if(Catch2_FOUND)
    set(_cet_Catch2_config_mode CONFIG_MODE)
  else()
    unset(_cet_Catch2_config_mode)
  endif()
endif()
if(NOT Catch2_FOUND)
  find_file(
    _cet_Catch2_hpp
    NAMES catch.hpp
    HINTS ENV CATCH_INC
    PATH_SUFFIXES catch catch2
    )
  if(_cet_Catch2_hpp)
    get_filename_component(_cet_Catch2_include_dir "${_cet_Catch2_hpp}" PATH)
    get_filename_component(
      _cet_Catch2_include_dir "${_cet_Catch2_include_dir}" PATH
      )
    if(_cet_Catch2_include_dir STREQUAL "/")
      unset(_cet_Catch2_include_dir)
    endif()
  endif()
  if(EXISTS "${_cet_Catch2_include_dir}")
    set(Catch2_FOUND TRUE)
    set(CATCH2_FOUND TRUE)
  endif()
endif()
if(Catch2_FOUND
   AND _cet_Catch2_include_dir
   AND NOT TARGET Catch2::Catch2
   )
  add_library(Catch2::Catch2 INTERFACE IMPORTED)
  set_target_properties(
    Catch2::Catch2 PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
                              "${_cet_Catch2_include_dir}"
    )
endif()

set(Catch2_FIND_REQUIRED ${_cet_Catch2_FIND_REQUIRED})
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  Catch2 ${_cet_Catch2_config_mode} REQUIRED_VARS Catch2_FOUND
  )

unset(_cet_Catch2_FIND_REQUIRED)
unset(_cet_Catch2_hpp CACHE)
unset(_cet_Catch2_config_mode)
