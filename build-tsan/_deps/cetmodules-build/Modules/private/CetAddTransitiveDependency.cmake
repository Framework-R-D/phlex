#[================================================================[.rst:
X
-
#]================================================================]

include_guard()

# Keep track of transitive dependencies.
function(_cet_add_transitive_dependency SOURCE_CALL FIRST_ARG)
  # Deal with optional leading COMPONENT <component> ourselves, as with
  # cmake_parse_arguments() we'd have to worry about what we might have passed
  # to find_package().
  if(FIRST_ARG STREQUAL "COMPONENT")
    list(POP_FRONT ARGN COMPONENT DEP)
    set(cache_var
        CETMODULES_FIND_DEPS_COMPONENT_${COMPONENT}_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
        )
    set(docstring_extra " component ${COMPONENT}")
  else()
    set(DEP "${FIRST_ARG}")
    set(cache_var
        CETMODULES_FIND_DEPS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
        )
    unset(docstring_extra)
  endif()
  if(NOT
     DEFINED
     CACHE{CETMODULES_FIND_DEPS_PNAMES_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}}
     )
    set(CETMODULES_FIND_DEPS_PNAMES_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
        "${DEP}"
        CACHE
          INTERNAL
          "Transitive dependency project names for ${CETMODULES_CURRENT_PROJECT_NAME}"
        )
  else()
    get_property(
      tdeps
      CACHE
        CETMODULES_FIND_DEPS_PNAMES_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
      PROPERTY VALUE
      )
    list(APPEND tdeps "${DEP}")
    list(REMOVE_DUPLICATES tdeps)
    set_property(
      CACHE
        CETMODULES_FIND_DEPS_PNAMES_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
      PROPERTY VALUE "${tdeps}"
      )
  endif()
  if(_fp_NO_EXPORT_${DEP}) # May be set by overridden find_package()
    # Don't need to add any find_dependency() calls.
    return()
  endif()
  # Set up the beginning of the call.
  string(TOLOWER "${SOURCE_CALL}" TRANSITIVE_CALL)
  if(TRANSITIVE_CALL STREQUAL "find_package")
    set(TRANSITIVE_CALL "find_dependency")
    foreach(mm IN ITEMS MIN MAX)
      if(NOT "${${DEP}_FIND_VERSION_${mm}_EXTRA}" STREQUAL "")
        list(
          APPEND
          ${cache_var}
          "set(${DEP}_FIND_VERSION_${mm}_EXTRA \"${${DEP}_FIND_VERSION_${mm}_EXTRA}\")"
          )
      endif()
    endforeach()
  endif()
  string(JOIN " " tmp ${DEP} ${ARGN})
  list(APPEND ${cache_var} "${TRANSITIVE_CALL}(${tmp})")
  if(NOT DEFINED CACHE{${cache_var}})
    set(${cache_var}
        "${${cache_var}}"
        CACHE
          INTERNAL
          "Transitive dependency directives for ${CETMODULES_CURRENT_PROJECT_NAME}\
${docstring_extra}\
"
        )
  else()
    set_property(CACHE ${cache_var} PROPERTY VALUE "${${cache_var}}")
  endif()
endfunction()
