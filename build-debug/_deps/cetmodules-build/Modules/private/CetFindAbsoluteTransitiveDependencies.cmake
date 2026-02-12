#[================================================================[.rst:
X
-
#]================================================================]

include_guard()

cmake_minimum_required(VERSION 3.20...4.1 FATAL_ERROR)

function(_cet_find_absolute_transitive_dependencies _cet_target_file_name
         _cet_target_file_data _ignore_regex
         )
  set(_stp_failed_targets)
  set(_stp_failed_targets_msg)
  # Check for unwanted absolute dependencies.
  string(REPLACE ";" "\\;" _absolute_transitive_dependencies_targets
                 "${_cet_target_file_data}"
         )
  # Isolate target property-setting commands with possibly-problematic
  # transitive dependencies.
  string(
    REGEX
      MATCHALL
      "(set_target_properties[ \t]*\\(|set_property[ \t*\\([ \t\r\n]*TARGET)([^ \t\r\n]+)[ \t\r\n]+PROPERTIES[^)]+INTERFACE_LINK_LIBRARIES[ \t\r\n]+\"([^\"]+;)?/[^)]+\\)"
      _absolute_transitive_dependencies_targets
      "${_absolute_transitive_dependencies_targets}"
    )
  if(NOT _absolute_transitive_dependencies_targets STREQUAL "")
    # Extract the information in a way that allows us to examine
    # possibly-problematic dependencies more closely.
    string(
      REGEX
      REPLACE
        "(^|;)(set_target_properties[ \t]*\\(|set_property[ \t]*\\([ \t\r\n]*TARGET)[ \t\r\n]*([^ \t\r\n]+)[^)]+INTERFACE_LINK_LIBRARIES[ \t\r\n]+\"([^\"]+)\"[^)]+\\)[^;]*"
        "\\3\t\\4\n"
        _absolute_transitive_dependencies_targets
        "${_absolute_transitive_dependencies_targets}"
      )
    foreach(_stp_call IN LISTS _absolute_transitive_dependencies_targets)
      if(_stp_call MATCHES "^([^\t]+)\t(.*)$")
        set(_stp_target "${CMAKE_MATCH_1}")
        set(_stp_libs "${CMAKE_MATCH_2}")
        # Anything that doesn't start with / is fine.
        list(FILTER _stp_libs INCLUDE REGEX "^/")
        # Assume anything under (/usr)?/lib(32|64)? is expected to be found
        # everywhere.
        list(FILTER _stp_libs EXCLUDE REGEX "^(/usr)?/lib(32|64)?/")
        if(_ignore_regex) # User-provided ignore regex.
          list(FILTER _stp_libs EXCLUDE REGEX "${_ignore_regex}")
        endif()
        if(_stp_libs)
          math(EXPR _stp_failed_targets "${_stp_failed_targets} + 1")
          # Format the failure message for this target.
          string(REGEX REPLACE "." " " _stp_target_indent "${_stp_target}")
          string(REGEX REPLACE ";" "\n${_stp_target_indent}    " _stp_libs
                               "${_stp_libs}"
                 )
          string(APPEND _stp_failed_targets_msg
                 "  ${_stp_target}: ${_stp_libs}\n"
                 )
        endif()
      endif()
    endforeach()
  endif()
  if(_stp_failed_targets)
    if(${_stp_failed_targets} EQUAL 1)
      set(_stp_targets_word "target")
    else()
      set(_stp_targets_word "targets")
    endif()
    # Error out on this export set.
    message(
      FATAL_ERROR
        "Found ${_stp_failed_targets} ${_stp_targets_word} with absolute dependencies in ${_cet_target_file_name}:\n${_stp_failed_targets_msg}Write a Find module and/or use find_package(... EXPORT) instead of find_library(). If certain paths must be ignored (because they are expected to be found on the system, for example), set project variable IGNORE_ABSOLULTE_DEPENDENCIES_REGEX for this project. To permit absolute paths in transitive dependencies unconditionally, set the boolean project variable IGNORE_ABSOLUTE_TRANSITIVE_DEPENDENCIES."
      )
  endif()
endfunction()

if(CETMODULES_TEST_TARGET)
  # Handle placeholders in target definitions.
  file(READ "${CET_TEST_TARGET}" _targetFileData)
  _cet_find_absolute_transitive_dependencies(
    "${CET_TEST_TARGET}" "${_targetFileData}" ""
    )
endif()
