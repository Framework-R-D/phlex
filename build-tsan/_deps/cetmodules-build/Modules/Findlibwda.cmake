#[================================================================[.rst:
Findlibwda
----------
  find libwda

#]================================================================]
if(libwda_FIND_REQUIRED)
  set(_cet_libwda_FIND_REQUIRED ${libwda_FIND_REQUIRED})
  unset(libwda_FIND_REQUIRED)
else()
  unset(_cet_libwda_FIND_REQUIRED)
endif()
find_package(libwda CONFIG QUIET)
if(_cet_libwda_FIND_REQUIRED)
  set(libwda_FIND_REQUIRED ${_cet_libwda_FIND_REQUIRED})
  unset(_cet_libwda_FIND_REQUIRED)
endif()
if(libwda_FOUND)
  set(_cet_libwda_config_mode CONFIG_MODE)
else()
  unset(_cet_libwda_config_mode)
  find_file(
    _cet_wda_h
    NAMES wda.h
    HINTS ENV LIBWDA_INC
    )
  if(_cet_wda_h)
    get_filename_component(_cet_libwda_include_dir "${_cet_wda_h}" PATH)
    if(_cet_libwda_include_dir STREQUAL "/")
      unset(_cet_libwda_include_dir)
    endif()
  endif()
  if(EXISTS "${_cet_libwda_include_dir}")
    set(libwda_FOUND TRUE)
    set(LIBWDA_FOUND TRUE)
    get_filename_component(_cet_libwda_dir "${_cet_libwda_include_dir}" PATH)
    if(_cet_libwda_dir STREQUAL "/")
      unset(_cet_libwda_dir)
    endif()
    set(libwda_INCLUDE_DIRS "${_cet_libwda_include_dir}")
    set(libwda_LIBRARY_DIR "${_cet_libwda_dir}/lib")
    find_library(
      libwda_LIBRARY
      NAMES wda
      PATHS ${libwda_LIBRARY_DIR} REQUIRED
      )
  endif()
endif()
if(libwda_FOUND)
  if(NOT TARGET wda::wda)
    add_library(wda::wda SHARED IMPORTED)
    set_target_properties(
      wda::wda PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${libwda_INCLUDE_DIRS}"
                          IMPORTED_LOCATION "${libwda_LIBRARY}"
      )
    set(libwda_LIBRARY "wda::wda")
  endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  libwda ${_cet_libwda_config_mode}
  REQUIRED_VARS libwda_FOUND libwda_INCLUDE_DIRS libwda_LIBRARY
  )

unset(_cet_libwda_FIND_REQUIRED)
unset(_cet_libwda_config_mode)
unset(_cet_libwda_dir)
unset(_cet_libwda_include_dir)
unset(_cet_wda_h CACHE)
