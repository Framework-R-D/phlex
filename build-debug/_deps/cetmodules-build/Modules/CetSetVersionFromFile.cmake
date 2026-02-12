#[================================================================[.rst:
CetSetVersionFromFile
---------------------

#]================================================================]
include_guard()

cmake_minimum_required(VERSION 3.20...4.1 FATAL_ERROR)

#[============================================================[.rst:
.. command:: cet_set_version_from_file

   Set the version of the specified or current project from a file.

   .. code-block:: cmake

      cet_set_version_from_file([<options>]...)

   Options
   ^^^^^^^

     ``EXTENDED_VERSION_SEMANTICS``
       Place code in CMake Config files to handle non-numeric version
       components.

     ``NOP``
       Optional separator between a list option and non-option
       arguments; no other effect.

     ``PROJECT``
       The project whose version is to be set. Defaults to the current
       project if not specified.

     ``VERSION_FILE``
       The file from which to read the version. Defaults to ``VERSION``
       if not specified.

   .. note:: Leading and trailing whitespace will be trimmed, including
      any trailing newline.

   .. versionadded:: 3.04

#]============================================================]
function(cet_set_version_from_file)
  cmake_parse_arguments(
    PARSE_ARGV 0 CSVF "EXTENDED_VERSION_SEMANTICS;NOP" "PROJECT;VERSION_FILE"
    ""
    )
  if(NOT CSVF_PROJECT)
    set(CSVF_PROJECT ${PROJECT_NAME})
  endif()
  if(NOT CSVF_VERSION_FILE)
    set(CSVF_VERSION_FILE "${${CSVF_PROJECT}_SOURCE_DIR}/VERSION")
  endif()
  file(READ "${CSVF_VERSION_FILE}" version_string)
  string(STRIP "${version_string}" version_string)
  set(${CSVF_PROJECT}_CMAKE_PROJECT_VERSION_STRING
      "${version_string}"
      PARENT_SCOPE
      )
  if(CSVF_EXTENDED_VERSION_SEMANTICS)
    set(${PROJECT_NAME}_EXTENDED_VERSION_SEMANTICS
        TRUE
        PARENT_SCOPE
        )
  endif()
endfunction()
