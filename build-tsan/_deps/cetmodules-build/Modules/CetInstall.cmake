#[================================================================[.rst:
CetInstall
----------

This module defines an implementation of a generic file / installation
function with the following features:

* Identify files to install and/or exclude by :command:`file(GLOB)
  <cmake-ref-current:command:file(glob)>`, or by list.

* Install contents of specific subdirectories:
  a) to their correct relative place in the install hierarchy; or
  b) to a base directory.

* Optionally, copy files for use in the build tree in addition to
  installing them.

\ :command:`_cet_install` is a "toolkit" function, intended to
facilitate the generation of ``install_X()`` functions with particular
default or enforced characteristics for files of a particular type.

#]================================================================]
# Avoid unwanted repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

include(CetCMakeUtils)
include(CetCopy)
include(private/CetExclude)

#[============================================================[.rst:
.. command:: _cet_install

   Install files of a particular type or category with installation
   semantics common to that type of file.

   .. code-block:: cmake

      _cet_install(<category> [<meta-options>...] [<options>...])

   .. _p_cet_install-options:

   Options
   ^^^^^^^

   .. include:: /opts-inc.rst

   .. _p_cet_install-meta-options:

   Meta-options
   """"""""""""

   ``_EXTRA_BASENAME_EXCLUDES [<basename-exclude-exp> ...]``
     Additional basename exclusion expressions.

   ``_EXTRA_EXCLUDES [<exclude-exp> ...]``
     Additional full-path exclusion expressions.

   ``_EXTRA_EXTRAS [<path>...]``
     Files to install in addition to those found via :command:`GLOB
     <cmake-ref-current:command:file(glob)>` expressions.

   .. _p_cet_install-_GLOB:

   ``_GLOBS [<file(glob)>...]``
     \ :command:`GLOB <cmake-ref-current:command:file(glob)>`
     expressions for files to include.

   ``_INSTALLED_FILES_VAR <var>``
     The name of a variable in which to stored the full list of files
     installed.

   ``_INSTALL_ONLY``
     Do not copy files to the build tree.

   ``_LIST_ONLY``
     Disable globbing: enforce explicit lists of files to install via
     :ref:`LIST <p_cet_install-opt-LIST>`.

   ``_NO_LIST``
     Disallow the use of :ref:`LIST <p_cet_install-opt-LIST>`.

   ``_SEARCH_BUILD``
     :command:`GLOB <cmake-ref-current:command:file(glob)>` expressions
     will be applied to the build tree in addition to the source tree.

   ``_SQUASH_SUBDIRS``
     Subdirectory elements of source files are ignored when calculating
     the copy/install destination.

   Details
   ^^^^^^^

   \ :ref:`Meta-options <p_cet_install-meta-options>` to
   :command:`!cet_install` are distinguished by a leading underscore and
   are intended for use by wrapper functions specific to a particular
   category of file (e.g. license and README files, geometry data,
   configuration files, *etc*.) to enforce common behavior for all
   installation operations for those files.

   .. note:: Although supported for historical reasons, use of
      :command:`file(GLOB) <cmake-ref-current:command:file(glob)>` to
      generate targets is not CMake best practice, and may lead to
      hysteresis if looking for generated files in the build tree.

#]============================================================]

# Copy all files (or PROGRAMS) found matching provided glob expressions to
# location indicated by DEST_VAR. Optionally (as indicated by _SEARCH_BUILD) use
# ${CMAKE_CURRENT_BINARY_DIR} as a relative base in addition to
# ${CMAKE_CURRENT_SOURCE_DIR}.
#
# If _SQUASH_SUBDIRS is not specified, any matching items found in SUBDIRS will
# be copied to a corresponding directory under the destination.
#
# Any non-absolute EXCLUDES are anchored to ${CMAKE_CURRENT_SOURCE_DIR}, while
# EXCLUDE_BASENAMES apply anywhere.
#
# Specified EXTRAS will be copied to the location indicatd by DEST_VAR without
# being checked for exclusion.
function(_cet_install CATEGORY)
  # Parse an option to tell us how to parse options.
  cmake_parse_arguments(PARSE_ARGV 1 _I "_LIST_ONLY;_NO_LIST" "" "")
  set(_USAGE_FLAGS)
  if(_I__LIST_ONLY)
    list(APPEND _USAGE_FLAGS _LIST_ONLY)
  elseif(_I__NO_LIST)
    list(APPEND _USAGE_FLAGS _NO_LIST)
  endif()
  # Set up main parsing operation.
  set(options PROGRAMS _INSTALL_ONLY)
  set(one_arg_options DEST_VAR DESTINATION SUBDIRNAME _INSTALLED_FILES_VAR)
  set(multi_arg_options)
  if(NOT _I__NO_LIST)
    list(APPEND multi_arg_options LIST)
  endif()
  if(NOT _I__LIST_ONLY)
    list(APPEND options GLOB)
    list(APPEND options _SEARCH_BUILD _SQUASH_SUBDIRS)
    list(
      APPEND
      multi_arg_options
      _EXTRA_BASENAME_EXCLUDES
      _EXTRA_EXCLUDES
      _EXTRA_EXTRAS
      BASENAME_EXCLUDES
      EXCLUDES
      EXTRAS
      _GLOBS
      SUBDIRS
      )
  endif()
  cmake_parse_arguments(
    _I "${options}" "${one_arg_options}" "${multi_arg_options}"
    "${_I_UNPARSED_ARGUMENTS}"
    )
  cet_passthrough(FLAG _I_PROGRAMS PROGRAMS)
  cet_passthrough(FLAG _I__INSTALL_ONLY _INSTALL_ONLY)
  # Act on options and arguments.
  if(NOT _I_DEST_VAR)
    if(_I_DESTINATION)
      set(CUSTOM_DESTINATION "${_I_DESTINATION}")
      set(_I_DEST_VAR CUSTOM_DESTINATION)
    else()
      list(POP_FRONT _I_UNPARSED_ARGUMENTS _I_DEST_VAR)
    endif()
  endif()
  # Sanity check.
  if(NOT (_I_DEST_VAR AND ${_I_DEST_VAR}))
    _cet_install_error(
      ${CATEGORY} ${_USAGE_FLAGS} "vacuous destination ${_I_DEST_VAR}"
      )
  endif()
  set(DEST_DIR "${${_I_DEST_VAR}}")

  if((_I__SEARCH_BUILD
      OR _I_BASENAME_EXCLUDES
      OR _I_EXCLUDES
      OR _I_EXTRAS
      OR _I_SUBDIRS
     )
     AND _I_LIST
     )
    _cet_install_error(
      ${CATEGORY} ${_USAGE_FLAGS} "mutually exclusive options detected"
      )
  elseif(_I_UNPARSED_ARGUMENTS)
    if(_I_LIST)
      _cet_install_error(
        ${CATEGORY} ${_USAGE_FLAGS}
        "unwanted extra arguments ${_I_UNPARSED_ARGUMENTS}"
        )
    else()
      warn_deprecated(
        "<file>... as non-option arguments to install_${CATEGORY}()" NEW
        "install_${CATEGORY}(LIST ...)"
        )
      set(_I_LIST "${_I_UNPARSED_ARGUMENTS}")
    endif()
  elseif(NOT _I_LIST)
    # Loop over subdirs.
    if(_I__GLOBS)
      foreach(
        SUBDIR IN
        ITEMS ""
        LISTS _I_SUBDIRS
        )
        set(GLOBS ${_I__GLOBS})
        if(SUBDIR)
          list(TRANSFORM GLOBS PREPEND "${SUBDIR}/")
        endif()
        file(
          GLOB FILES
          LIST_DIRECTORIES FALSE
          CONFIGURE_DEPENDS ${GLOBS}
          )
        if(_I__SEARCH_BUILD)
          # Search build area for files.
          list(TRANSFORM GLOBS PREPEND "${CMAKE_CURRENT_BINARY_DIR}/")
          file(
            GLOB TMP
            LIST_DIRECTORIES FALSE
            CONFIGURE_DEPENDS ${GLOBS}
            )
          list(APPEND FILES ${TMP})
        endif()
        if(FILES)
          # Process exclusions.
          _cet_exclude_from_list(
            FILES
            LIST
            ${FILES}
            EXCLUDES
            ${_I_EXCLUDES}
            ${_I__EXTRA_EXCLUDES}
            BASENAME_EXCLUDES
            ${_I_BASENAME_EXCLUDES}
            ${_I__EXTRA_BASENAME_EXCLUDES}
            )
          if(_I__SQUASH_SUBDIRS)
            # We can deal with everything in one operation.
            list(APPEND _I_EXTRAS ${FILES})
          else()
            if(_I_SUBDIRNAME)
              string(JOIN "/" tmp_subdir "${_I_SUBDIRNAME}" "${SUBDIR}")
            else()
              set(tmp_subdir "${SUBDIR}")
            endif()
            # One subdirectory at a time.
            _cet_install_list(
              ${CATEGORY}
              ${_I_DEST_VAR}
              ${PROGRAMS}
              ${_INSTALL_ONLY}
              _USAGE_FLAGS
              ${_USAGE_FLAGS}
              SUBDIRNAME
              "${tmp_subdir}"
              LIST
              ${FILES}
              )
            list(APPEND INSTALLED_FILES ${FILES})
          endif()
        endif()
      endforeach()
    endif()
  endif()
  # Deal with specified extras.
  _cet_install_list(
    ${CATEGORY}
    ${_I_DEST_VAR}
    ${PROGRAMS}
    ${_INSTALL_ONLY}
    _USAGE_FLAGS
    ${_USAGE_FLAGS}
    SUBDIRNAME
    ${_I_SUBDIRNAME}
    LIST
    ${_I_LIST}
    ${_I_EXTRAS}
    ${_I__EXTRA_EXTRAS}
    )
  list(APPEND INSTALLED_FILES ${_I_LIST} ${_I_EXTRAS} ${_I__EXTRA_EXTRAS})
  if(_I__INSTALLED_FILES_VAR)
    set(${_I__INSTALLED_FILES_VAR}
        ${INSTALLED_FILES}
        PARENT_SCOPE
        )
  endif()
endfunction()

function(_cet_install_error CATEGORY)
  cmake_parse_arguments(PARSE_ARGV 1 _CIE "_LIST_ONLY;_NO_LIST" "" "")
  set(PREAMBLE "Usage: ")
  string(REGEX REPLACE "." " " EMPTY "${PREAMBLE}")
  set(USAGE_MSG)
  if(NOT NO_LIST)
    string(
      APPEND
      USAGE_MSG
      "${PREAMBLE}${install_${CATEGORY}}([PROGRAMS] [SUBDIRNAME <subdir>] LIST ...)"
      )
    set(PREAMBLE "${EMPTY}")
  endif()
  if(NOT LIST_ONLY)
    string(
      APPEND
      USAGE_MSG
      "${PREAMBLE}install_${CATEGORY}([PROGRAMS] [SUBDIRNAME <subdir>]\n"
      "${EMPTY}   [BASENAME_EXCLUDES ...] [EXCLUDES ...]\n"
      "${EMPTY}   [EXTRAS ...] [SUBDIRS ...])"
      )
  endif()
  message(FATAL_ERROR "install_${CATEGORY}(): " ${_CIE_UNPARSED_ARGUMENTS}
                      "\n\n${USAGE_MSG}"
          )
endfunction()

# Copy listed files (or PROGRAMS) to location indicated by DEST_VAR (optionally
# under subdirectory SUBDIRNAME) in build tree and install tree.
#
# Relative paths for ${${DEST_VAR}} are with respect to
# ${CETMODULES_CURRENT_PROJECT_BINARY_DIR} or ${CMAKE_INSTALL_PREFIX} as
# appropriate, the optional subdirectory is relative to the resolved
# ${${DEST_VAR}}, and listed files are relative to ${CMAKE_CURRENT_SOURCE_DIR}.
function(_cet_install_list CATEGORY DEST_VAR)
  cmake_parse_arguments(
    PARSE_ARGV 2 _IL "_INSTALL_ONLY;PROGRAMS" "SUBDIRNAME" "_USAGE_FLAGS;LIST"
    )
  if(NOT ${DEST_VAR})
    _cet_install_error(
      ${CATEGORY} ${_IL__USAGE_FLAGS} "vacuous destination ${DEST_VAR}"
      )
  endif()
  if(_IL_PROGRAMS)
    set(IMODE PROGRAMS)
    set(CMODE PROGRAMS)
  else()
    set(IMODE FILES)
  endif()
  set(DEST_DIR "${${DEST_VAR}}")
  if(_IL_SUBDIRNAME)
    string(JOIN "/" DEST_DIR "${DEST_DIR}" "${_IL_SUBDIRNAME}")
  endif()
  if(NOT _IL__INSTALL_ONLY)
    cet_copy(${CMODE} DESTINATION
             "${CETMODULES_CURRENT_PROJECT_BINARY_DIR}/${DEST_DIR}"
             WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" ${_IL_LIST}
             )
  endif()
  install(${IMODE} ${_IL_LIST} DESTINATION "${DEST_DIR}")
endfunction()
