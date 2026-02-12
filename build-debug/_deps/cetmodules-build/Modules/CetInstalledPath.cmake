#[================================================================[.rst:
CetInstalledPath
----------------

Define the function :command:`cet_installed_path` to calculate an
installation path.

#]================================================================]

include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

include(CetPackagePath)
include(CetRegexEscape)

#[================================================================[.rst:
.. command:: cet_installed_path

   Calculate a path to a file or directory ``<path>`` as-installed.

   .. parsed-literal::

      cet_installed_path(<out-var> <path> { RELATIVE <rel-dir> | RELATIVE_VAR <project-var> } [<options>])

   Options
   ^^^^^^^

   ``BASE_SUBDIR <rel-base-dir>``
     Specify ``<rel-base-dir>`` as the location of the installed file
     relative to `<rel-dir>` or the content of |pv| ``<project-var>``.

   ``NOP``
     Option / argument disambiguator; no other function.

   ``RELATIVE <rel-dir>``
     Relative path ``<rel-dir>`` will be removed from the front of the
     calculated path if present.

   ``RELATIVE_VAR <project-var>``
     The relative path represented by the value of the project variable
     ``<PROJECT-NAME>_<project-var>`` will be removed from the front of
     the calculated path if present, accounting for
     :variable:`<PROJECT-NAME>_EXEC_PREFIX` where appropriate.

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<out-var>``
     The calculated path shall be returned in ``<out-var>``.

   ``<path>``
     The path to the file or directory for which the installation path
     should be calculated.

#]================================================================]

function(cet_installed_path OUT_VAR)
  cmake_parse_arguments(
    PARSE_ARGV 1 CIP "NOP" "BASE_SUBDIR;RELATIVE;RELATIVE_VAR" ""
    )
  list(POP_FRONT CIP_UNPARSED_ARGUMENTS PATH)
  if(CIP_RELATIVE AND CIP_RELATIVE_VAR)
    message(FATAL_ERROR "RELATIVE and RELATIVE_VAR are mutually exclusive")
  elseif(NOT (CIP_RELATIVE OR CIP_RELATIVE_VAR))
    message(FATAL_ERROR "one of RELATIVE or RELATIVE_VAR are required")
  endif()
  cet_package_path(pkg_path PATH "${PATH}")
  if(NOT pkg_path)
    set(pkg_path "${PATH}")
  endif()
  if(CIP_RELATIVE_VAR)
    if(NOT ${CIP_RELATIVE_VAR} IN_LIST
       CETMODULES_VARS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
       )
      message(
        FATAL_ERROR
          "RELATIVE_VAR ${CIP_RELATIVE_VAR} is not a project variable for project ${CETMODULES_CURRENT_PROJECT_NAME}"
        )
    endif()
    cet_regex_escape(
      "${${CETMODULES_CURRENT_PROJECT_NAME}_EXEC_PREFIX}" VAR e_exec_prefix
      )
    string(REGEX
           REPLACE "^(${e_exec_prefix}/+)?(.+)$" "\\2" relvar
                   "${${CETMODULES_CURRENT_PROJECT_NAME}_${CIP_RELATIVE_VAR}}"
           )
    cet_regex_escape("${relvar}" VAR e_relvar)
  else()
    cet_regex_escape("${CIP_RELATIVE}" VAR e_relvar)
  endif()
  string(REGEX REPLACE "^(${e_relvar}/+)?(.+)$" "\\2" result "${pkg_path}")
  if(IS_ABSOLUTE "${result}")
    message(
      FATAL_ERROR
        "unable to calculate relative install path for ${PATH} in project ${CETMODULES_CURRENT_PROJECT_NAME}"
      )
  endif()
  string(JOIN "/" result ${CIP_BASE_SUBDIR} "${result}")
  set(${OUT_VAR}
      "${result}"
      PARENT_SCOPE
      )
endfunction()
