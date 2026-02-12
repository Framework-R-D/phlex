#[================================================================[.rst:
X
-
#]================================================================]
include_guard()
cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

# Exclude items from a list of files.
#
# Non-absolute files and EXCLUDES are relative to BASE_DIR if specified (or
# CMAKE_CURRENT_SOURCE_DIR if not). BASENAME_EXCLUDES are applied only to the
# basenames of files and should *not* contain `/' directory separators. If any
# special globbing characters (`[', `]`, `*', `?', or `+') are present in a
# BASENAME_EXCLUDE member, it will be treated as a globbing pattern in the
# directory of every member of LIST.
function(_cet_exclude_from_list OUTPUT_VAR)
  cmake_parse_arguments(
    PARSE_ARGV 1 XL "" "BASE_DIR" "BASENAME_EXCLUDES;EXCLUDES;LIST"
    )
  set(GLOBS)
  set(BASENAME_EXCLUDES)
  foreach(myfile IN LISTS XL_BASENAME_EXCLUDES)
    if(${myfile} MATCHES "/")
      message(
        FATAL_ERROR "specified BASENAME_EXCLUDES ${myfile} is not a basename"
        )
    elseif(${myfile} MATCHES [=[[]+*?[]]=]) # Treat as a GLOB.
      list(APPEND GLOBS "${myfile}")
    else()
      list(APPEND BASENAME_EXCLUDES "${myfile}")
    endif()
  endforeach()
  set(EXCLUDES)
  # Get real paths for anchored exclusions.
  foreach(myfile IN LISTS XL_EXCLUDES)
    get_filename_component(myfile "${myfile}" REALPATH BASE_DIR ${XL_BASE_DIR})
    list(APPEND EXCLUDES ${myfile})
  endforeach()
  set(RESULTS)
  foreach(myfile IN LISTS XL_LIST)
    get_filename_component(myfile "${myfile}" REALPATH BASE_DIR ${XL_BASE_DIR})
    get_filename_component(mybasename "${myfile}" NAME)
    # Exclude literal basename exclusion matches.
    if(mybasename IN_LIST BASENAME_EXCLUDES)
      continue() # Don't want this one.
    endif()
    get_filename_component(mydir "${myfile}" DIRECTORY)
    if(GLOBS)
      set(local_globs ${GLOBS})
      list(TRANSFORM local_globs PREPEND "${mydir}/")
      file(
        GLOB matches
        LIST_DIRECTORIES FALSE
        ${local_globs}
        )
      # Exclude globbed basename matches.
    else()
      set(matches)
    endif()
    if(NOT (myfile IN_LIST matches))
      list(APPEND RESULTS "${myfile}")
    endif()
  endforeach()
  # Exclude anchored exclusions.
  list(REMOVE_ITEM RESULTS ITEMS ${EXCLUDES})
  # Publish.
  set(${OUTPUT_VAR}
      "${RESULTS}"
      PARENT_SCOPE
      )
endfunction()
