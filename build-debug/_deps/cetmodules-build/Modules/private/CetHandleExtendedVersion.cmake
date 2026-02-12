#[================================================================[.rst:
X
-
#]================================================================]

include_guard()

include(ParseVersionString)

macro(_cet_handle_extended_version)
  if(DEFINED PROJECT_VERSION)
    parse_version_string(
      "${PROJECT_VERSION}" _hev_VERSION_MAJOR _hev_VERSION_MINOR
      _hev_VERSION_PATCH _hev_VERSION_TWEAK
      )
    if(NOT
       (PROJECT_VERSION_MAJOR STREQUAL "${_hev_VERSION_MAJOR}"
        AND PROJECT_VERSION_MINOR STREQUAL "${_hev_VERSION_MINOR}"
        AND PROJECT_VERSION_PATCH STREQUAL "${_hev_VERSION_PATCH}"
        AND PROJECT_VERSION_TWEAK STREQUAL "${_hev_VERSION_TWEAK}")
       )
      message(
        WARNING
          "\
mismatch between PROJECT_VERSION and derived components PROJECT_VERSION_MAJOR, \
etc. - manual setting for PROJECT_VERSION after project() call?
"
        )
    endif()
    unset(_hev_VERSION_MAJOR)
    unset(_hev_VERSION_MINOR)
    unset(_hev_VERSION_PATCH)
    unset(_hev_VERSION_TWEAK)
  endif()

  if(${CETMODULES_CURRENT_PROJECT_NAME}_CMAKE_PROJECT_VERSION_STRING
     AND NOT ${CETMODULES_CURRENT_PROJECT_NAME}_CMAKE_PROJECT_VERSION_STRING
         STREQUAL PROJECT_VERSION
     )
    message(
      VERBOSE
      "\
resetting PROJECT_VERSION and friends to match \
${CETMODULES_CURRENT_PROJECT_NAME}_CMAKE_PROJECT_VERSION_STRING \
(${${CETMODULES_CURRENT_PROJECT_NAME}_CMAKE_PROJECT_VERSION_STRING})\
"
      )

    set(PROJECT_VERSION
        "${${CETMODULES_CURRENT_PROJECT_NAME}_CMAKE_PROJECT_VERSION_STRING}"
        )
    set(${CETMODULES_CURRENT_PROJECT_NAME}_VERSION "${PROJECT_VERSION}")

    parse_version_string(
      "${PROJECT_VERSION}" ${CETMODULES_CURRENT_PROJECT_NAME}_VERSION_INFO
      )

    set(_hev_prefix_args IN ITEMS PROJECT LISTS CETMODULES_CURRENT_PROJECT_NAME)

    if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
      set(CMAKE_PROJECT_VERSION "${PROJECT_VERSION}")
      list(APPEND _hev_prefix_args ITEMS CMAKE_PROJECT)
    endif()
    foreach(_hev_prefix ${_hev_prefix_args})
      parse_version_string(
        ${CETMODULES_CURRENT_PROJECT_NAME}_VERSION_INFO
        ${_hev_prefix}_VERSION_MAJOR
        ${_hev_prefix}_VERSION_MINOR
        ${_hev_prefix}_VERSION_PATCH
        ${_hev_prefix}_VERSION_TWEAK
        ${_hev_prefix}_VERSION_EXTRA
        )
    endforeach()
    unset(_hev_prefix)
    unset(_hev_prefix_args)
  endif()
endmacro()
