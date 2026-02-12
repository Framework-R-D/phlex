#[================================================================[.rst:
CetCMakeUtils
-------------

General functions and macros.

#]================================================================]

include_guard()

cmake_minimum_required(VERSION 3.20...4.1 FATAL_ERROR)

include(CetRegexEscape)
include(CetRegisterExportSet)
include(ProjectVariable)

#[================================================================[.rst:
.. command:: cet_passthrough

   Turn a flag or option value into something that can be passed on to
   another function or macro.

   .. code-block:: cmake

      cet_passthrough([FLAG] [KEYWORD <keyword>]
                      [EMPTY_KEYWORD <empty-keyword>] <in_var>
                      IN_PLACE|<out-var>)

      cet_passthrough([FLAG] VALUES [<val>...]
                      KEYWORD <keyword>
                      [EMPTY_KEYWORD <empty-keyword>] <out-var>)

   Options
   ^^^^^^^

   ``EMPTY_KEYWORD <empty-keyword>``
     If ``<in-var>`` or ``VALUES`` evaluates to the empty string, the
     result is ``<empty-keyword>``

   ``FLAG``
     If ``<in-var>`` or ``VALUES`` evaluates to ``TRUE``, the result is
     ``<keyword>`` (or see ``KEYWORD``, below). Otherwise the result
     will be the empty string (or see ``EMPTY_KEYWORD``, above).

   ``IN_PLACE``
     If ``<in-var>`` is specified this option signifies that the result
     will be placed in ``<in-var>``. In this case ``<out-var>`` must
     then *not* be present.

   ``KEYWORD <keyword>``
     If specified, the option keyword will be ``<keyword>``. Otherwise,
     if ``<in-var>`` is specified, then it will be the name
     "``<in-var>``" with any leading ``<prefix>_`` stripped off the
     front. Failing that, the name "``<out-var>``" will be used as the
     default.

   ``VALUES <val>...``
     The values to be passed through to another function or macro may be
     specified as ``<val>...`` rather than as ``<in-var>``. In this
     case, ``<out-var>`` is required and ``IN_PLACE`` is not permitted.

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<in-var>``
     The name of a variable holding the values to be passed
     through. ``<in-var>`` must *not* be present if ``VALUES`` is
     specified.

   ``<out-var>``
     The name of a variable to hold the values in passthrough form. If
     ``IN_PLACE`` and ``<in-var>`` are both specified, than
     ``<out-var>`` must *not* be present.

   Examples
   ^^^^^^^^

   * .. code-block:: cmake

        set(MYOPTS_VERBOSE TRUE)
        cet_passthrough(FLAG IN_PLACE MYOPTS_VERBOSE)

     ``MYOPTS_VERBOSE`` will have the value "VERBOSE" in the calling
     function or macro.

   * .. code-block:: cmake

        cet_passthrough(FLAG VALUES "NOTFOUND" USE_MYPACKAGE)

     ``USE_MYPACKAGE`` will be empty in the calling function or macro.

   * .. code-block:: cmake

        cet_passthrough(FLAG VALUES "MYTEXT" USE_MYPACKAGE)

     ``USE_MYPACKAGE`` will have the value ``USE_MYPACKAGE`` in the
     calling function or macro.

   * .. code-block:: cmake

        cet_passthrough(IN_PLACE VALUES
          "Mary had a little lamb; Its fleece was white as snow"
          KEYWORD RHYME MARY_LAMB)

     The list ``MARY_LAMB`` will consist of the **three** elements:

     .. code-block:: console

        "RHYME" "Mary had a little lamb" "Its fleece was white as snow"

     in the calling function or macro. Note the lack of whitespace at
     the beginning of the third element of the list.

   * .. code-block:: cmake

        cet_passthrough(VALUES
          "Mary had a little lamb\\\\; Its fleece was white as snow"
          KEYWORD RHYME MARY_LAMB)

     The list ``MARY_LAMB`` will consist of the **two** elements:

     .. code-block:: console

        "RHYME" "Mary had a little lamb; Its fleece was white as snow"

     in the calling function or macro.

#]================================================================]

function(cet_passthrough)
  cmake_parse_arguments(
    PARSE_ARGV 0 CP "APPEND;FLAG;IN_PLACE" "EMPTY_KEYWORD;KEYWORD" "VALUES"
    )
  if(NOT (CP_VALUES OR "VALUES" IN_LIST CP_KEYWORDS_MISSING_VALUES))
    list(POP_FRONT CP_UNPARSED_ARGUMENTS CP_IN_VAR)
    if(CP_IN_VAR
       MATCHES
       "^(ARG[VN]|CP_(APPEND|EMPTY_KEYWORD|IN_PLACE|IN_VAR|KEYWORD|KEYWORDS_MISSING_VALUES|OUT_VAR|UNPARSED_ARGUMENTS|VALUES))$"
       )
      message(
        FATAL_ERROR
          "value of IN_VAR non-option argument (\"${CP_IN_VAR}\") is \
not permitted - specify values with VALUES instead\
"
        )
    elseif(NOT CP_IN_VAR)
      message(
        FATAL_ERROR "vacuous <in-var> non-option argument - missing VALUES?"
        )
    elseif(NOT (CP_KEYWORD OR "KEYWORD" IN_LIST CP_KEYWORDS_MISSING_VALUES))
      string(REGEX REPLACE "^_*[^_]+_(.*)$" "\\1" CP_KEYWORD "${CP_IN_VAR}")
    endif()
    if(CP_IN_PLACE)
      if(CP_APPEND)
        message(
          FATAL_ERROR "options IN_PLACE and APPEND are mutually exclusive"
          )
      endif()
      set(CP_OUT_VAR "${CP_IN_VAR}")
    endif()
  elseif(CP_IN_PLACE)
    message(FATAL_ERROR "options IN_PLACE and VALUES are mutually exclusive")
  else()
    set(CP_IN_VAR CP_VALUES)
  endif()
  if(NOT CP_OUT_VAR)
    list(POP_FRONT CP_UNPARSED_ARGUMENTS CP_OUT_VAR)
    if(NOT CP_OUT_VAR)
      message(FATAL_ERROR "vacuous OUT_VAR non-option argument")
    endif()
  endif()
  if(NOT (CP_KEYWORD OR "KEYWORD" IN_LIST CP_KEYWORDS_MISSING_VALUES))
    set(CP_KEYWORD "${CP_OUT_VAR}")
  endif()
  if(CP_UNPARSED_ARGUMENTS)
    message(
      FATAL_ERROR "unexpected non-option arguments ${CP_UNPARSED_ARGUMENTS}"
      )
  endif()
  if(CP_FLAG)
    if(CP_APPEND)
      if(${CP_IN_VAR})
        list(APPEND ${CP_OUT_VAR} ${CP_KEYWORD})
      elseif(CP_EMPTY_KEYWORD)
        list(APPEND ${CP_OUT_VAR} ${CP_EMPTY_KEYWORD})
      endif()
      set(${CP_OUT_VAR}
          "${${CP_OUT_VAR}}"
          PARENT_SCOPE
          )
    else()
      if(${CP_IN_VAR})
        set(${CP_OUT_VAR}
            ${CP_KEYWORD}
            PARENT_SCOPE
            )
      elseif(CP_EMPTY_KEYWORD)
        set(${CP_OUT_VAR}
            ${CP_EMPTY_KEYWORD}
            PARENT_SCOPE
            )
      else()
        unset(${CP_OUT_VAR} PARENT_SCOPE)
      endif()
    endif()
  elseif(NOT DEFINED ${CP_IN_VAR} OR "${${CP_IN_VAR}}" STREQUAL "")
    if(CP_APPEND)
      list(APPEND ${CP_OUT_VAR} ${CP_EMPTY_KEYWORD})
      set(${CP_OUT_VAR}
          "${${CP_OUT_VAR}}"
          PARENT_SCOPE
          )
    elseif(CP_EMPTY_KEYWORD)
      set(${CP_OUT_VAR}
          ${CP_EMPTY_KEYWORD}
          PARENT_SCOPE
          )
    endif()
  elseif(CP_APPEND)
    list(APPEND ${CP_OUT_VAR} ${CP_KEYWORD} ${${CP_IN_VAR}})
    set(${CP_OUT_VAR}
        "${${CP_OUT_VAR}}"
        PARENT_SCOPE
        )
  else()
    set(${CP_OUT_VAR}
        ${CP_KEYWORD} ${${CP_IN_VAR}}
        PARENT_SCOPE
        )
  endif()
endfunction()

#[================================================================[.rst:
.. command:: cet_source_file_extensions

   Produce an ordered list of source file extensions for enabled
   languages.

   .. code-block:: cmake

      cet_source_file_extensions(<out-var>)

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<out-var>``
     Variable to contain the resulting ordered list of extensions.

   Notes
   ^^^^^

   .. note:: Prescribed order of enabled languages: ``CUDA``, ``CXX``
      ``C``, ``Fortran``, ``<lang>...``, ``ASM``.

#]================================================================]
function(cet_source_file_extensions RESULTS_VAR)
  set(RESULTS)
  get_property(enabled_languages GLOBAL PROPERTY ENABLED_LANGUAGES)
  # Specific order.
  list(
    REMOVE_ITEM
    enabled_languages
    ASM
    Fortran
    C
    CXX
    CUDA
    )
  list(PREPEND enabled_languages CUDA CXX C Fortran)
  list(APPEND enabled_languages ASM)
  foreach(lang IN LISTS enabled_languages)
    if(CMAKE_${lang}_COMPILER_LOADED)
      list(APPEND RESULTS ${CMAKE_${lang}_SOURCE_FILE_EXTENSIONS})
    endif()
  endforeach()
  set(${RESULTS_VAR}
      "${RESULTS}"
      PARENT_SCOPE
      )
endfunction()

#[================================================================[.rst:
.. command:: cet_exclude_files_from

   Remove duplicates and other files from a list, explicitly or by
   regular expression.

   .. code-block:: cmake

      cet_exclude_files_from(<sources-var> [REGEX <regex>...] [NOP] <file>...)

   Options
   ^^^^^^^

   ``NOP``
     Optional separator between a list option and non-option arguments;
     no other effect.

   ``REGEX``
     Entries in ``<sources-var>`` matching ``<regex>...`` will be
     removed.

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<sources-var>``
     The name of a variable containing a list of files to be pruned.

   ``<file>...``
     Files to be removed from ``<sources-var>`` (exact matches only).

   Notes
   ^^^^^

   .. note:: Relative paths are interpreted with respect to the value of
      :variable:`CMAKE_CURRENT_SOURCE_DIR
      <cmake-ref-current:variable:CMAKE_CURRENT_SOURCE_DIR>`.

#]================================================================]
function(cet_exclude_files_from SOURCES_VAR)
  if(NOT (${SOURCES_VAR} AND ARGN)) # Nothing to do.
    return()
  endif()
  # Remove known plugin sources and anything else the user specifies.
  cmake_parse_arguments(PARSE_ARGV 1 CEFF "NOP" "" "REGEX")
  if(CEFF_REGEX)
    list(JOIN CEFF_REGEX "|" regex)
    list(FILTER ${SOURCES_VAR} EXCLUDE REGEX "(${regex})")
  endif()
  if(CEFF_UNPARSED_ARGUMENTS)
    # Transform relative paths with respect to the current source directory.
    list(
      TRANSFORM CEFF_UNPARSED_ARGUMENTS
      PREPEND "${CMAKE_CURRENT_SOURCE_DIR}/"
      REGEX [=[^[^/]]=]
      )
    # Remove exact matches only.
    list(REMOVE_ITEM ${SOURCES_VAR} ${CEFF_UNPARSED_ARGUMENTS})
  endif()
  list(REMOVE_DUPLICATES ${SOURCES_VAR})
  set(${SOURCES_VAR}
      "${${SOURCES_VAR}}"
      PARENT_SCOPE
      )
endfunction()

#[================================================================[.rst:
.. command:: cet_timestamp

   Generate a current timestamp.

     .. code-block:: cmake

        cet_timestamp(<out-var> [SYSTEM_DATE_CMD] [<fmt>])

   Options
   ^^^^^^^

   .. _cet_timestamp-SYSTEM_DATE_CMD:

   ``SYSTEM_DATE_CMD``
     Use the system :manpage:`date(1)` command even if ``<fmt>`` is
     understood by :command:`string(TIMESTAMP)
     <cmake-ref-current:command:string(timestamp)>`

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<out-var>``
     Variable in which to store the formatted timestamp.

   ``<fmt>``
     The desired format of the timestamp, using ``%`` placeholders
     understood by :command:`string(TIMESTAMP)
     <cmake-ref-current:command:string(timestamp)>` or the system
     :manpage:`date(1)` command.

   Examples
   ^^^^^^^^

   * .. code-block:: cmake

        cet_timestamp(RESULT)
        message(STATUS "${RESULT}")

     .. code-block:: console

        -- Sun Jan 01 23:59:59 CST 1970

   * .. code-block:: cmake

        cet_timestamp(RESULT "%Y-%m-%d %H:%M:%S %z")
        message(STATUS "${RESULT}")

     .. code-block:: console

        -- 1970-01-01 23:59:59 -0600

   Notes
   ^^^^^

   .. versionchanged:: 2.07.00

      ``%Y`` was missing from the default format in earlier versions.

   .. versionchanged:: 3.08.00

      added :ref:`SYSTEM_DATE_CMD <cet_timestamp-SYSTEM_DATE_CMD>`.

   .. seealso:: :command:`string(TIMESTAMP) <cmake-ref-current:command:string(timestamp)>`, :manpage:`date(1)`

#]================================================================]
function(cet_timestamp VAR)
  cmake_parse_arguments(PARSE_ARGV 1 _CT "SYSTEM_DATE_CMD" "" "")
  list(POP_FRONT ARGN fmt)
  if(NOT fmt)
    set(fmt "%a %b %d %H:%M:%S %Z %Y")
  endif()
  if(_CT_SYSTEM_DATE_CMD OR fmt MATCHES "(^|[^%])%[^%dHIjmbBMsSUwaAyY]")
    # We want to use the system date command explicitly, or there's a format
    # code not recognized by string(TIMESTAMP).
    set(date_cmd date "+${fmt}")
    execute_process(
      COMMAND ${date_cmd}
      OUTPUT_VARIABLE result
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_VARIABLE error
      RESULT_VARIABLE status
      )
    if(error OR NOT (status EQUAL 0 AND result))
      message(
        WARNING
          "attempt to obtain date/time with \"${date_cmd}\" \
returned status code ${status} and error output \"${error}\" in addition to output \"${result}\"\
"
        )
    endif()
  else()
    string(TIMESTAMP result "${fmt}")
  endif()
  set(${VAR}
      "${result}"
      PARENT_SCOPE
      )
endfunction()

#[================================================================[.rst:
.. command:: cet_find_simple_package

   :command:`find_package() <cmake-ref-current:command:find_package>` for
   packages without generated CMake config files or a ``Find<name>.cmake``
   module.

   .. deprecated:: 2.0

      if no ``FindXXX.cmake`` module or CMake config file is available
      for ``<name>``, write your own find module or request one from the
      SciSoft team.

      .. seealso:: :manual:`cmake-packages(7) <cmake-ref-current:manual:cmake-packages(7)>`

   .. code-block:: cmake

      find_simple_package([HEADERS <header>...]
        [INCPATH_SUFFIXES <dir>...] [INCPATH_VAR <var>]
        [LIB_VAR <var>] [LIBNAMES <libname>...]
        [LIBPATH_SUFFIXES <dir>...]
        <name>)

   Options
   ^^^^^^^

   ``HEADERS <header>...``
     Look for ``<header>...`` to ascertain the include path. If not
     specified, use ``<name>.{h,hh,H,hxx,hpp}``

   ``INCPATH_SUFFIXES <dir>...``
     Add ``<suffix>...`` to paths when searching for headers (defaults
     to "include").

   ``INCPATH_VAR <var>``
     Store the found include path in ``<var>``. If not specified, we
     invoke :command:`include_directories()
     <cmake-ref-current:command:include_directories>` with the found
     include path.

   ``LIB_VAR <var>``
     Store the found library as ``<var>``. If not specified, use
     ``<name>`` as converted to an upper case identifier.

   ``LIBNAMES <libname>...``
     Look for ``<libname>...`` as a library in addition to ``name``.

   ``LIBPATH_SUFFIXES <dir>...``
     Add ``<dir>...`` to paths when searching for libraries.

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<name>``
     The primary name of the library (without prefix or suffix) or
     headers to be found.

#]================================================================]

#[================================================================[.rst:
.. command:: cet_localize_pv

   Ensure that specified path-type :manual:`project variables
   <cetmodules-project-variables(7)>` are absolute in the current
   directory scope for in-tree project ``<project>``.

   .. code-block:: cmake

      cet_localize_pv(<project> [<options>] ALL|[<project-var-name>...])

   .. rst-class:: text-start

   .. seealso:: :command:`cet_localize_pv_all`,
                :manual:`cetmodules-project-variables(7)`,
                :command:`project_variable`.

   Options
   ^^^^^^^

   ``BINARY|SOURCE|TRY_BINARY``
      .. deprecated:: 4.0

      These options are deprecated due to possible ambiguity of meaning:
      use option :ref:`BINARY_ONLY <_cet_localize_pv-BINARY_ONLY>`,
      :ref:`SOURCE_ONLY <_cet_localize_pv-SOURCE_ONLY>`, or
      :ref:`PREFER_BINARY <_cet_localize_pv-PREFER_BINARY>` instead,
      respectively.

   .. _cet_localize_pv-BINARY_ONLY:

   ``BINARY_ONLY``
     Unconditionally resolve non-absolute paths with respect to the
     binary tree (mutually exclusive with ``SOURCE_ONLY`` and
     ``PREFER_BINARY``).

   ``NO_CHECK_VALIDITY``
     Do not report an error if a project variable does not exist, or
     does not represent a path or path fragment (ignored for ``ALL``).

   .. _cet_localize_pv-PREFER_BINARY:

   ``PREFER_BINARY``
     Try to resolve a non-absolute path with respect to the binary
     rather than the source tree (default for ``FILEPATH`` and
     ``FILEPATH_FRAGMENT`` :ref:`project-variables-types`). If the path
     does not resolve to an existing or :prop_sf:`GENERATED
     <cmake-ref-current:prop_sf:GENERATED>` :manual:`CMake source file
     <cmake-ref-current:manual:cmake-properties(7)>`, it will be
     resolved with respect to the source tree. This option is mutually
     exclusive with ``BINARY_ONLY`` and ``SOURCE_ONLY``.

   .. _cet_localize_pv-SOURCE_ONLY:

   ``SOURCE_ONLY``
     Unconditionally resolve non-absolute paths with respect to the
     source tree (mutually exclusive with ``BINARY_ONLY`` and
     ``PREFER_BINARY``).

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<project>``
     The name of a CMake project in the current source tree.

   ``ALL|<project-var-name>...``
     The name of one or more project variables (without a ``<project>_``
     prefix).

   Notes
   ^^^^^

   .. versionchanged:: 4.00.00

      Deprecated ``BINARY|SOURCE|TRY_BINARY`` options in favor of their
      respective replacements.

#]================================================================]
function(cet_localize_pv PROJECT)
  if(NOT ${PROJECT}_IN_TREE)
    return() # Nothing to do.
  endif()
  cmake_parse_arguments(
    PARSE_ARGV
    1
    CLPV
    "BINARY;BINARY_ONLY;NO_CHECK_VALIDITY;PREFER_BINARY;SOURCE;SOURCE_ONLY;TRY_BINARY"
    ""
    ""
    )
  if(CLPV_SOURCE)
    warn_deprecated("SOURCE" SINCE 4.00.00 NEW "SOURCE_ONLY")
    set(CLPV_SOURCE_ONLY TRUE)
  elseif(CLPV_BINARY)
    warn_deprecated("BINARY" SINCE 4.00.00 NEW "BINARY_ONLY")
    set(CLPV_BINARY_ONLY TRUE)
  elseif(CLPV_TRY_BINARY)
    warn_deprecated("TRY_BINARY" SINCE 4.00.00 NEW "PREFER_BINARY")
    set(CLPV_PREFER_BINARY TRUE)
  endif()
  set(n_exc)
  foreach(opt IN ITEMS BINARY_ONLY PREFER_BINARY SOURCE_ONLY)
    if(CLPV_${opt})
      math(EXPR n_exc "${n_exc} + 1")
    endif()
  endforeach()
  if(n_exc GREATER 1)
    message(
      FATAL_ERROR
        "options BINARY_ONLY, SOURCE_ONLY and PREFER_BINARY are mutually exclusive"
      )
  endif()
  set(check_pv_validity)
  if(CLPV_UNPARSED_ARGUMENTS STREQUAL "ALL")
    set(var_list "CETMODULES_VARS_PROJECT_${PROJECT}")
  else()
    set(var_list CLPV_UNPARSED_ARGUMENTS)
    if(NOT CLPV_NO_CHECK_VALIDITY)
      set(check_pv_validity TRUE)
    endif()
  endif()
  foreach(var IN LISTS ${var_list})
    if(NOT var IN_LIST "CETMODULES_VARS_PROJECT_${PROJECT}")
      if(check_pv_validity)
        message(
          SEND_ERROR
            "cannot localize unknown project variable ${var} for project ${PROJECT}"
          )
      endif()
      continue()
    endif()
    set(result)
    cet_get_pv_property(pv_type PROJECT ${PROJECT} ${var} PROPERTY TYPE)
    if(NOT pv_type MATCHES [=[^(FILE)?PATH(_FRAGMENT)$]=])
      if(check_pv_validity)
        message(
          SEND_ERROR
            "cannot localize non-path project variable ${var} for project ${PROJECT}"
          )
      endif()
      continue()
    endif()
    foreach(item IN ITEMS $CACHE{${PROJECT}_${var}})
      if(NOT CLPV_SOURCE_ONLY)
        get_filename_component(
          binary_path "${item}" ABSOLUTE BASE_DIR "${${PROJECT}_BINARY_DIR}"
          )
        get_property(
          binary_generated
          SOURCE "${binary_path}"
          PROPERTY GENERATED
          )
        if(NOT
           (binary_path
            AND (EXISTS "${binary_path}" OR binary_generated)
            OR CLPV_BINARY_ONLY)
           )
          unset(binary_path)
        endif()
        if(CLPV_BINARY_ONLY OR (CLPV_PREFER_BINARY AND binary_path))
          list(APPEND result "${binary_path}")
          continue()
        endif()
      endif()
      get_filename_component(
        source_path "${${PROJECT}_${var}}" ABSOLUTE BASE_DIR
        "${${PROJECT}_SOURCE_DIR}"
        )
      if(NOT ((source_path AND EXISTS "${source_path}")) OR CLPV_SOURCE_ONLY)
        unset(source_path)
      endif()
      if(source_path OR CLPV_SOURCE_ONLY)
        list(APPEND result "${source_path}")
      elseif(binary_path)
        list(APPEND result "${binary_path}")
      else()
        if(check_pv_validity)
          message(
            SEND_ERROR
              "requested localization of path project variable ${var} for project ${PROJECT} is not present and is not known to be generated"
            )
        endif()
        list(APPEND result "")
      endif()
    endforeach()
    set(${PROJECT}_${var}
        "${result}"
        PARENT_SCOPE
        )
  endforeach()
endfunction()

#[================================================================[.rst:
.. command:: cet_localize_pv_all

   Equivalent to :command:`cet_localize_pv(\<project> [BINARY|SOURCE|TRY_BINARY] ALL)
   <cet_localize_pv>`.

   .. code-block:: cmake

      cet_localize_pv_all(<project> [BINARY|SOURCE|TRY_BINARY])

   .. seealso:: :command:`cet_localize_pv`.

#]================================================================]

function(cet_localize_pv_all PROJECT)
  if(ARGN
     AND NOT ARGN MATCHES
         "^(BINARY|BINARY_ONLY|SOURCE|SOURCE_ONLY|TRY_BINARY|PREFER_BINARY)$"
     )
    message(WARNING "unrecognized extra arguments ${ARGN}")
    set(ARGN)
  endif()
  cet_localize_pv(${PROJECT} ALL ${ARGN})
endfunction()

#[================================================================[.rst:
.. command:: cet_cmake_module_directories

   Make the specified CMake module-containing directories available in
   the current directory scope via :variable:`CMAKE_MODULE_PATH
   <cmake-ref-current:variable:CMAKE_MODULE_PATH>` and to client
   packages via :command:`find_package()
   <cmake-ref-current:command:find_package>`.

   .. code-block:: cmake

      cet_cmake_module_directories([NO_CONFIG] [NO_LOCAL] [PROJECT <project>] <dir>...)

   Options
   ^^^^^^^

   ``BINARY``
     Also add the corresponding directories in the project build tree to
     :variable:`CMAKE_MODULE_PATH
     <cmake-ref-current:variable:CMAKE_MODULE_PATH>` in the current
     scope (e.g. for generated CMake modules).

     .. rst-class:: text-start

     .. seealso:: :command:`configure_file()
                  <cmake-ref-current:command:configure_file>`,
                  :command:`cet_make_plugin_builder`,
                  :command:`cet_collect_plugin_builders`.

   .. _cet_cmake_module_directories-NO_CONFIG:

   ``NO_CONFIG``
     Do not add these directories to :variable:`CMAKE_MODULE_PATH
     <cmake-ref-current:variable:CMAKE_MODULE_PATH>` in the CMake config
     file for ``<project>``.

     .. seealso:: :ref:`NO_LOCAL
                  <cet_cmake_module_directories-NO_LOCAL>`

   .. _cet_cmake_module_directories-NO_LOCAL:

   ``NO_LOCAL``
     Do not add these directories to :variable:`CMAKE_MODULE_PATH
     <cmake-ref-current:variable:CMAKE_MODULE_PATH>` in the current
     scope. Implied if ``<project>`` is not equal to the value of
     :variable:`CETMODULES_CURRENT_PROJECT_NAME`

     .. seealso:: :ref:`NO_CONFIG
                  <cet_cmake_module_directories-NO_CONFIG>`

   ``PROJECT <project>``
     Specify the project to which these module directories belong. If
     not specifed, ``<project>`` defaults to
     :variable:`CETMODULES_CURRENT_PROJECT_NAME
     <CETMODULES_CURRENT_PROJECT_NAME>`.

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<dir>...``
     Directories containing CMake modules.

#]================================================================]
function(cet_cmake_module_directories)
  cmake_parse_arguments(
    PARSE_ARGV 0 CMD "BINARY;NO_CONFIG;NO_LOCAL" "PROJECT" ""
    )
  if(CMD_PROJECT)
    if(NOT CMD_PROJECT STREQUAL CETMODULES_CURRENT_PROJECT_NAME)
      set(NO_LOCAL TRUE)
    endif()
  else()
    set(CMD_PROJECT "${CETMODULES_CURRENT_PROJECT_NAME}")
  endif()
  if(NOT CMD_NO_LOCAL)
    list(
      TRANSFORM CMD_UNPARSED_ARGUMENTS
      PREPEND "${CETMODULES_CURRENT_PROJECT_SOURCE_DIR}/"
      REGEX "^[^/]+" OUTPUT_VARIABLE tmp_installed
      )
    if(CMD_BINARY)
      list(
        TRANSFORM CMD_UNPARSED_ARGUMENTS
        PREPEND "${CETMODULES_CURRENT_PROJECT_BINARY_DIR}/"
        REGEX "^[^/]+" OUTPUT_VARIABLE tmp_local
        )
    else()
      unset(tmp_local)
    endif()
    list(PREPEND CMAKE_MODULE_PATH ${tmp_installed} ${tmp_local})
    set(CMAKE_MODULE_PATH
        "${CMAKE_MODULE_PATH}"
        PARENT_SCOPE
        )
  endif()
  if(NOT CMD_NO_CONFIG)
    if(NOT DEFINED
       CACHE{CETMODULES_CMAKE_MODULES_DIRECTORIES_PROJECT_${CMD_PROJECT}}
       )
      set(CETMODULES_CMAKE_MODULES_DIRECTORIES_PROJECT_${CMD_PROJECT}
          ${CMD_UNPARSED_ARGUMENTS}
          CACHE INTERNAL
                "CMAKE_MODULE_PATH additions for ${CMD_PROJECT}Config.cmake"
          )
    else()
      set_property(
        CACHE CETMODULES_CMAKE_MODULES_DIRECTORIES_PROJECT_${CMD_PROJECT}
        APPEND
        PROPERTY VALUE ${CMD_UNPARSED_ARGUMENTS}
        )
    endif()
  endif()
endfunction()

#[============================================================[.rst:
.. command:: cet_export_alias

   Define and export aliases of the specified targets into the specified
   or default export set.

   .. deprecated:: 3.15 in favor of :command:`cet_make_alias()`, which
   is more flexible, more robust against user error and makes fewer
   assumptions.

   .. code-block:: cmake

      cet_export_alias([<options>] [ALIAS] <target>...)

   Options
   ^^^^^^^

   .. _cet_export_alias_ALIAS:

   ``ALIAS <target>...``
     Optional keyword specifying targets to be aliased. If non-option
     arguments are also specified, they will be appended to the list
     specified here.

   ``ALIAS_NAMESPACE <namespace>``
     Targets specified without a namespace (via ``::``) will be taken
     from ``<namespace>``.

   ``EXPORT_SET <export-set>``
     Aliased targets will be exported into ``<export-set>``, which will
     be created if necessary with :command:`cet_register_export_set` and
     an appropriate default namespace based on ``<export-set>``. If you
     require a different namespace than the default, call
     :command:`cet_register_export_set` yourself prior to calling
     :command:`cet_export_alias`.

   ``NOP``
     Optional separator between a list option and non-option
     arguments; no other effect.

  Non-option arguments
  ^^^^^^^^^^^^^^^^^^^^

  ``<target>...``
    Targets to be aliased.

    .. seealso:: :ref:`ALIAS <cet_export_alias_ALIAS>`

  Notes
  ^^^^^

  .. note:: If no ``<export-set>`` is specified, the default export set
     will be used.

  .. seealso:: :command:`cet_register_export_set`,
               :manual:`cmake-packages(7)
               <cmake-ref-current:manual:cmake-packages(7)>`

#]============================================================]
function(cet_export_alias)
  warn_deprecated("cet_export_alias()" NEW "cet_make_alias()")
  cmake_parse_arguments(
    PARSE_ARGV 0 _cea "NOP" "ALIAS_NAMESPACE;EXPORT_SET" "ALIAS"
    )
  set(default_export_namespace)
  set(export_namespace)
  if(_cea_UNPARSED_ARGUMENTS)
    list(APPEND _cea_ALIAS ${_cea_UNPARSED_ARGUMENTS})
  endif()
  if(_cea_EXPORT_SET)
    cet_register_export_set(
      SET_NAME ${_cea_EXPORT_SET} NAMESPACE_VAR export_namespace NO_REDEFINE
      )
  endif()
  if(_cea_ALIAS_NAMESPACE)
    list(
      TRANSFORM _cea_ALIAS
      PREPEND ${_cea_ALIAS_NAMESPACE}::
      REGEX "^([^:]|:([^:]|$))*$"
      ) # Only transform aliases without ::
  endif()
  foreach(alias IN LISTS _cea_ALIAS)
    get_property(
      primary_exported_target
      TARGET ${alias}
      PROPERTY EXPORT_NAME
      )
    if(NOT primary_exported_target)
      get_property(
        primary_exported_target
        TARGET ${alias}
        PROPERTY ALIASED_TARGET
        )
    endif()
    if(alias MATCHES "^(.*)::(.*)$")
      if(export_namespace)
        set(export_alias "${export_namespace}::${CMAKE_MATCH_2}")
      else()
        set(export_alias "${alias}")
      endif()
      string(PREPEND primary_exported_target "${CMAKE_MATCH_1}::")
    else()
      if(NOT default_export_namespace)
        cet_register_export_set(NAMESPACE_VAR default_export_namespace)
      endif()
      if(export_namespace)
        set(export_alias "${export_namespace}::${alias}")
      else()
        set(export_alias "${default_export_namespace}::${alias}")
      endif()
      string(PREPEND primary_exported_target "${default_exported_namespace}::")
    endif()
    _cet_export_import_cmd(
      TARGETS
      ${alias}
      COMMANDS
      "if (TARGET ${primary_exported_target})
  add_library(${export_alias} ALIAS ${primary_exported_target})
endif()\
"
      )
  endforeach()
endfunction()

#[============================================================[.rst:
.. command:: cet_make_alias

   Define an alias and optionally export it.

   .. code-block:: cmake

      cet_make_alias(TARGET <target> [<options>] [<target-export-set>])

   Options
   ^^^^^^^

   ``EXPORT_SET <export-set>``
     Aliased targets will be exported into ``<export-set>``, which will
     be created if necessary with :command:`cet_register_export_set` and
     an appropriate default namespace based on ``<export-set>``. If you
     require a different namespace than the default, call
     :command:`cet_register_export_set` yourself prior to calling
     :command:`cet_export_alias`. If no ``<export-set>`` is specified,
     the default export set will be used.

   ``NAME <name>``
     Specify the alias as ``<name>``. If ``<name>`` is namespaced, then
     it will not be exported, and the presence of ``EXPORT_SET`` is an
     error. Without ``NAME`` the alias will have the same root name as
     ``<target>``.

   ``NOP``
     Optional separator between a list option and non-option arguments;
     no other effect.

   .. _cet_make_alias_TARGET:

   ``TARGET <target>``
     Target to be aliased.

   ``TARGET_EXPORT_SET <target-export-set>``
     If the primary target (after resolving all aliases) represented by
     ``<target>`` may be found in multiple export sets, specify the
     correct one here.

  Notes
  ^^^^^

  .. seealso:: :command:`cet_register_export_set`,
               :manual:`cmake-packages(7)
               <cmake-ref-current:manual:cmake-packages(7)>`

#]============================================================]
function(cet_make_alias)
  cmake_parse_arguments(
    PARSE_ARGV 0 _cma "NOP" "NAME;EXPORT_SET;TARGET;TARGET_EXPORT_SET" ""
    )
  if(DEFINED _cma_UNPARSED_ARGUMENTS)
    message(
      FATAL_ERROR "unrecognized unparsed arguments ${_cma_UNPARSED_ARGUMENTS}"
      )
  endif()
  set(export_namespace)
  if(NOT "${_cma_EXPORT_SET}" STREQUAL "")
    if(_cma_NAME MATCHES "::")
      message(
        FATAL_ERROR
          "<name> cannot be namespace-scoped when <export_set> is specified"
        )
    endif()
    cet_register_export_set(
      SET_NAME ${_cma_EXPORT_SET} NAMESPACE_VAR export_namespace NO_REDEFINE
      )
  elseif(NOT _cma_NAME MATCHES "::")
    cet_register_export_set(NAMESPACE_VAR export_namespace)
  endif()
  if("${_cma_NAME}" STREQUAL "")
    string(REGEX REPLACE "^.*::" "" _cma_NAME "${_cma_TARGET}")
  endif()
  set(primary_target)
  set(current_target "${_cma_TARGET}")
  while("${primary_target}" STREQUAL "")
    get_property(
      next_target
      TARGET "${current_target}"
      PROPERTY ALIASED_TARGET
      )
    if("${next_target}" STREQUAL "")
      get_property(
        primary_target
        TARGET "${current_target}"
        PROPERTY EXPORT_NAME
        )
      if("${primary_target}" STREQUAL "")
        set(primary_target "${current_target}")
      endif()
    else()
      set(current_target "${next_target}")
    endif()
  endwhile()
  string(JOIN "::" export_alias ${export_namespace} ${_cma_NAME})
  add_library(${export_alias} ALIAS ${current_target})
  if(export_namespace)
    if("${_cma_TARGET_EXPORT_SET}" STREQUAL "")
      foreach(
        export_set_candidate IN
        LISTS ${CETMODULES_CURRENT_PROJECT_NAME}_DEFAULT_EXPORT_SET
              CETMODULES_EXPORT_SETS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
        )
        if("${primary_target}"
           IN_LIST
           CETMODULES_TARGET_EXPORT_NAMES_EXPORT_SET_${export_set_candidate}_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
           )
          list(APPEND _cma_TARGET_EXPORT_SET "${export_set_candidate}")
        endif()
      endforeach()
      list(REMOVE_DUPLICATES _cma_TARGET_EXPORT_SET)
      if("${_cma_TARGET_EXPORT_SET}" STREQUAL "")
        message(
          FATAL_ERROR
            "cannot export an alias to a non-exported primary target (${primary_target})"
          )
      elseif("${_cma_TARGET_EXPORT_SET}" MATCHES ";")
        message(
          FATAL_ERROR
            "ambiguity: primary target ${primary_target} (resolved from ${_cma_TARGET}) \
found in multiple export sets: ${_cma_TARGET_EXPORT_SET} - use _CMA_TARGET_EXPORT_SET to resolve ambiguity\
"
          )
      endif()
    elseif(
      NOT
      "${primary_target}"
      IN_LIST
      CETMODULES_TARGET_EXPORT_NAMES_EXPORT_SET_${_cma_TARGET_EXPORT_SET}_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
      )
      message(
        FATAL_ERROR
          "primary target ${primary_target} (resolved from ${_cma_TARGET}) \
not found in specified export set ${_cma_TARGET_EXPORT_SET}"
        )
    endif()
    cet_register_export_set(
      NAMESPACE_VAR target_export_namespace ${_cma_TARGET_EXPORT_SET}
      NO_REDEFINE
      )
    string(JOIN "::" export_target ${target_export_namespace} ${primary_target})
    _cet_export_import_cmd(
      TARGETS
      ${export_alias}
      COMMANDS
      "if (TARGET ${export_target})
  add_library(${export_alias} ALIAS ${export_target})
endif()\
"
      )
  endif()
endfunction()

#[============================================================[.rst:
.. command:: cet_real_path

   Convert provided paths to a :envvar:`PATH`-like string or CMake list
   of real paths via :command:`file(REAL_PATH)
   <cmake-ref-current:command:file(real_path)>`.

   .. code-block:: cmake

      cet_real_path(<out-var> [LIST] <path>...)

   Options
   ^^^^^^^

   ``LIST``
     Results will be returned in ``<out-var>`` as a ";"-separated CMake
     list rather than the default ":"-separated :envvar:`PATH`-like
     string.

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<out-var>``
      Variable to hold the results.

   ``<path>``
     Input paths for conversion.

#]============================================================]
function(cet_real_path OUT_VAR)
  cmake_parse_arguments(PARSE_ARGV 1 CRP "LIST" "" "")
  set(result)
  string(REPLACE ":" ";" items "${CRP_UNPARSED_ARGUMENTS}")
  foreach(
    item IN
    LISTS
    items
    )
    file(REAL_PATH "${item}" item)
    list(APPEND result "${item}")
  endforeach()
  if(NOT CRP_LIST)
    string(REPLACE ";" ":" result "${result}")
  endif()
  set(${OUT_VAR}
      "${result}"
      PARENT_SCOPE
      )
endfunction()

#[============================================================[.rst:
.. command:: cet_filter_subdirs

   Filter paths based on their roots.

   .. code-block:: cmake

      cet_filter_subdirs([EXCLUDE <subdir>...] [INCLUDE <subdir>...] <path>...)

   Options
   ^^^^^^^

   ``(EXCLUDE|INCLUDE) <root>...``
     Exclude or select ``<path>`` based on whether it is a subdirectory
     of ``<root>`` after accounting for symbolic links.

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<path>...``
     Candidate paths to be filtered.

#]============================================================]
function(cet_filter_subdirs OUT_VAR)
  cmake_parse_arguments(PARSE_ARGV 1 CFS "NOP" "" "EXCLUDE;INCLUDE")
  if(DEFINED CFS_EXCLUDE AND DEFINED CFS_INCLUDE)
    message(FATAL_ERROR "EXCLUDE and INCLUDE options are mutually exclusive")
  endif()
  string(REPLACE ":" ";" CFS_UNPARSED_ARGUMENTS "${CFS_UNPARSED_ARGUMENTS}")
  set(selected)
  set(remainder)
  if(DEFINED CFS_EXCLUDE)
    set(mode EXCLUDE)
  elseif(DEFINED CFS_INCLUDE)
    set(mode INCLUDE)
  else() # Short-circuit.
    set(mode)
    set(selected
        "${CFS_UNPARSED_ARGUMENTS}"
        PARENT_SCOPE
        )
    unset(CFS_UNPARSED_ARGUMENTS)
  endif()
  foreach(raw_candidate IN LISTS CFS_UNPARSED_ARGUMENTS)
    file(REAL_PATH "${raw_candidate}" candidate EXPAND_TILDE)
    cmake_path(GET candidate FILENAME end_dir)
    cet_regex_escape("${end_dir}" end_dir_re)
    set(found)
    foreach(root IN LISTS CFS_EXCLUDE CFS_INCLUDE)
      file(REAL_PATH "${root}" root EXPAND_TILDE)
      cmake_path(IS_PREFIX root "${candidate}" found)
      if(found)
        break()
      endif()
      file(
        GLOB_RECURSE pool FOLLOW_SYMLINKS
        LIST_DIRECTORIES TRUE
        "${root}/${end_dir}" "${root}/*/${end_dir}"
        )
      foreach(pool_item IN LISTS pool)
        file(REAL_PATH "${pool_item}" pool_item)
        if(pool_item MATCHES "/${end_dir_re}$")
          cmake_path(COMPARE "${pool_item}" EQUAL "${candidate}" found)
          if(found)
            break()
          endif()
        endif()
      endforeach()
      if(found)
        break()
      endif()
    endforeach()
    if(found)
      list(APPEND selected "${raw_candidate}")
    else()
      list(APPEND remainder "${raw_candidate}")
    endif()
  endforeach()
  if(mode STREQUAL "EXCLUDE")
    set(${OUT_VAR}
        "${remainder}"
        PARENT_SCOPE
        )
  else()
    set(${OUT_VAR}
        "${selected}"
        PARENT_SCOPE
        )
  endif()
endfunction()

if(CMAKE_SCRIPT_MODE_FILE) # Smoke test.
  cet_passthrough(
    KEYWORD RHYME MARY_LAMB
    "Mary had a little lamb\\; Its fleece was white as snow"
    )
  list(LENGTH MARY_LAMB len)
  if(NOT len EQUAL 2)
    message(FATAL_ERROR "MARY_LAMB has ${len} elements - expected 2")
  endif()
endif()

function(_cet_export_import_cmd)
  cmake_parse_arguments(PARSE_ARGV 0 _cc "NOP" "EXPORT_SET" "COMMANDS;TARGETS")
  if(NOT _cc_COMMANDS)
    set(_cc_COMMANDS "${_cc_UNPARSED_ARGUMENTS}")
  endif()
  string(REPLACE "\n" ";" _cc_COMMANDS "${_cc_COMMANDS}")
  if(DEFINED
     CACHE{CETMODULES_IMPORT_COMMANDS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}}
     )
    set_property(
      CACHE
        CETMODULES_IMPORT_COMMANDS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
      APPEND
      PROPERTY VALUE "${_cc_COMMANDS}"
      )
  else()
    set(CETMODULES_IMPORT_COMMANDS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
        "${_cc_COMMANDS}"
        CACHE
          INTERNAL
          "Convenience aliases for exported non-runtime targets for project ${CETMODULES_CURRENT_PROJECT_NAME}"
        )
  endif()
  _add_to_exported_targets(TARGETS ${_cc_TARGETS})
endfunction()

function(_add_to_exported_targets)
  cmake_parse_arguments(PARSE_ARGV 0 _add "" "EXPORT_SET" "TARGETS")
  if(NOT _add_TARGETS OR (NOT _add_EXPORT_SET AND "EXPORT_SET" IN_LIST
                                                  _add_KEYWORDS_MISSING_VALUES)
     )
    return()
  endif()
  if(_add_EXPORT_SET)
    set(cache_var
        CETMODULES_EXPORTED_TARGETS_EXPORT_SET_${_add_EXPORT_SET}_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
        )
    set(export_names)
    foreach(tgt IN LISTS _add_TARGETS)
      get_property(
        export_name
        TARGET ${tgt}
        PROPERTY EXPORT_NAME
        )
      if(export_name)
        list(APPEND export_names ${export_name})
      else()
        list(APPEND export_names ${tgt})
      endif()
    endforeach()
    if(DEFINED
       CACHE{CETMODULES_TARGET_EXPORT_NAMES_EXPORT_SET_${_add_EXPORT_SET}_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}}
       )
      set_property(
        CACHE
          CETMODULES_TARGET_EXPORT_NAMES_EXPORT_SET_${_add_EXPORT_SET}_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
        APPEND
        PROPERTY VALUE ${export_names}
        )
    else()
      set(CETMODULES_TARGET_EXPORT_NAMES_EXPORT_SET_${_add_EXPORT_SET}_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
          ${export_names}
          CACHE
            INTERNAL
            "List of export names for ${_add_EXPORT_SET} targets for project ${CETMODULES_CURRENT_PROJECT_NAME}"
          )
    endif()
  else()
    set(_add_EXPORT_SET manual)
    set(cache_var
        CETMODULES_EXPORTED_MANUAL_TARGETS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
        )
  endif()
  if(DEFINED CACHE{${cache_var}})
    set_property(
      CACHE ${cache_var}
      APPEND
      PROPERTY VALUE ${_add_TARGETS}
      )
  else()
    set(${cache_var}
        ${_add_TARGETS}
        CACHE
          INTERNAL
          "List of exported ${_add_EXPORT_SET} targets for project ${CETMODULES_CURRENT_PROJECT_NAME}"
        )
  endif()
endfunction()

set(CET_WARN_DEPRECATED TRUE)

function(warn_deprecated OLD)
  if(NOT CET_WARN_DEPRECATED)
    return()
  endif()
  cmake_parse_arguments(PARSE_ARGV 1 WD "" "NEW;SINCE" "")
  if(WD_NEW)
    set(msg " - use ${WD_NEW} instead")
  endif()
  if(NOT DEFINED WD_SINCE OR "SINCE" IN_LIST WD_KEYWORDS_MISSING_VALUES)
    set(WD_SINCE "cetmodules 2.10")
  elseif(WD_SINCE MATCHES "^((v|[Vv]ersion)[ 	]+)?[0-9][^ 	]*$")
    set(WD_SINCE "cetmodules ${WD_SINCE}")
  endif()
  if(NOT "${WD_SINCE}" STREQUAL "")
    string(PREPEND WD_SINCE " since ")
  endif()
  message(DEPRECATION "${OLD} is deprecated" "${WD_SINCE}" ${msg}
                      ${WD_UNPARSED_ARGUMENTS}
          )
endfunction()
