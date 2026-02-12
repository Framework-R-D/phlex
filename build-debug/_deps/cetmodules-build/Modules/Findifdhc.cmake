#[================================================================[.rst:
Findifdhc
----------
  find ifdhc

#]================================================================]
if(ifdhc_FIND_REQUIRED)
  set(_cet_ifdhc_FIND_REQUIRED ${ifdhc_FIND_REQUIRED})
  unset(ifdhc_FIND_REQUIRED)
else()
  unset(_cet_ifdhc_FIND_REQUIRED)
endif()
find_package(ifdhc CONFIG QUIET)
if(_cet_ifdhc_FIND_REQUIRED)
  set(ifdhc_FIND_REQUIRED ${_cet_ifdhc_FIND_REQUIRED})
  unset(_cet_ifdhc_FIND_REQUIRED)
endif()
if(ifdhc_FOUND)
  set(_cet_ifdhc_config_mode CONFIG_MODE)
else()
  unset(_cet_ifdhc_config_mode)
  find_file(
    _cet_ifdh_h
    NAMES ifdh.h
    HINTS ENV IFDHC_INC ENV IFDHC_DIR
    PATH_SUFFIXES inc
    )
  if(_cet_ifdh_h)
    get_filename_component(_cet_ifdhc_include_dir "${_cet_ifdh_h}" PATH)
    if(_cet_ifdhc_include_dir STREQUAL "/")
      unset(_cet_ifdhc_include_dir)
    endif()
  endif()
  if(EXISTS "${_cet_ifdhc_include_dir}")
    set(ifdhc_FOUND TRUE)
    set(IFDHC_FOUND TRUE)
    get_filename_component(_cet_ifdhc_dir "${_cet_ifdhc_include_dir}" PATH)
    if(_cet_ifdhc_dir STREQUAL "/")
      unset(_cet_ifdhc_dir)
    endif()
    set(ifdhc_INCLUDE_DIRS "${_cet_ifdhc_include_dir}")
    set(ifdhc_LIBRARY_DIR "${_cet_ifdhc_dir}/lib")
    find_library(
      ifdhc_LIBRARY
      NAMES ifdh
      PATHS ${ifdhc_LIBRARY_DIR} REQUIRED
      )
  endif()
endif()
if(ifdhc_FOUND)
  if(NOT TARGET ifdh::ifdh)
    add_library(ifdh::ifdh SHARED IMPORTED)
    set_target_properties(
      ifdh::ifdh
      PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${ifdhc_INCLUDE_DIRS}"
                 IMPORTED_LOCATION "${ifdhc_LIBRARY}"
      )
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  ifdhc ${_cet_ifdhc_config_mode} REQUIRED_VARS ifdhc_FOUND ifdhc_INCLUDE_DIRS
                                                ifdhc_LIBRARY
  )

unset(_cet_ifdhc_FIND_REQUIRED)
unset(_cet_ifdhc_config_mode)
unset(_cet_ifdhc_dir)
unset(_cet_ifdhc_include_dir)
unset(_cet_ifdh_h CACHE)
