find_program(JSONNET_EXE NAMES jsonnet)
mark_as_advanced(JSONNET_EXE)
if(JSONNET_EXE)
  execute_process(
    COMMAND ${JSONNET_EXE} --version
    OUTPUT_VARIABLE ${CMAKE_FIND_PACKAGE_NAME}_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
    )
  string(REGEX
         REPLACE "^.*v([0-9.]+)$" "\\1" ${CMAKE_FIND_PACKAGE_NAME}_VERSION
                 "${${CMAKE_FIND_PACKAGE_NAME}_VERSION}"
         )
  if(NOT ${CMAKE_FIND_PACKAGE_NAME}_INCLUDE_DIR)
    find_path(${CMAKE_FIND_PACKAGE_NAME}_INCLUDE_DIR NAMES libjsonnet++.h)
    mark_as_advanced(${CMAKE_FIND_PACKAGE_NAME}_INCLUDE_DIR)
  endif()
  find_library(${CMAKE_FIND_PACKAGE_NAME}_LIBRARY NAMES jsonnet++)
  mark_as_advanced(${CMAKE_FIND_PACKAGE_NAME}_LIBRARY)
  find_library(${CMAKE_FIND_PACKAGE_NAME}_CLIBRARY NAMES jsonnet)
  mark_as_advanced(${CMAKE_FIND_PACKAGE_NAME}_CLIBRARY)
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
  ${CMAKE_FIND_PACKAGE_NAME}
  REQUIRED_VARS ${CMAKE_FIND_PACKAGE_NAME}_INCLUDE_DIR
                ${CMAKE_FIND_PACKAGE_NAME}_LIBRARY
  VERSION_VAR ${CMAKE_FIND_PACKAGE_NAME}_VERSION
  )

if(${CMAKE_FIND_PACKAGE_NAME}_FOUND)
  set(${CMAKE_FIND_PACKAGE_NAME}_LIBRARIES
      ${${CMAKE_FIND_PACKAGE_NAME}_LIBRARY}
      )
  set(${CMAKE_FIND_PACKAGE_NAME}_INCLUDE_DIRS
      ${${CMAKE_FIND_PACKAGE_NAME}_INCLUDE_DIRS}
      )
  if(NOT TARGET jsonnet::lib)
    add_library(jsonnet::lib SHARED IMPORTED)
    set_target_properties(
      jsonnet::lib
      PROPERTIES IMPORTED_LOCATION "${${CMAKE_FIND_PACKAGE_NAME}_LIBRARY}"
                 INTERFACE_INCLUDE_DIRECTORIES
                 "${${CMAKE_FIND_PACKAGE_NAME}_INCLUDE_DIR}"
                 INTERFACE_LINK_LIBRARIES
                 ${${CMAKE_FIND_PACKAGE_NAME}_CLIBRARY}
      )
  endif()
endif()
