#[================================================================[.rst:
CetCopy
-------

Module defining the function :command:`cet_copy` to copy a file or files
to a destination directory as part of the build.

#]================================================================]

# Avoid unnecessary repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

include(CetPackagePath)

#[================================================================[.rst:
.. command:: cet_copy

   Copy one or more files as part of the build process (defining a
   target).

   .. parsed-literal::

      cet_copy(<source> DESTINATION <dir> :ref:`common options <cet_copy-common-options>` \
      :ref:`single-source options <cet_copy-single-source-options>`)

      cet_copy(<sources>... DESTINATION <dir> :ref:`common options <cet_copy-common-options>`)

   .. versionchanged:: 3.23.00

      Use of :ref:`single-source options
      <cet_copy-single-source-options>` for multiple sources is an
      error.

   Options
   ^^^^^^^

   ``DESTINATION <directory>``
     The destination directory. If relative, the destination shall be
     calculated relative to :variable:`CMAKE_CURRENT_BINARY_DIR
     <cmake-ref-current:variable:CMAKE_CURRENT_BINARY_DIR>`

   .. _cet_copy-common-options:

   Common options
   """"""""""""""

   ``DEPENDENCIES <dep>...``
     If ``<dep>`` changes, ``<sources>`` shall be considered out-of-date
     (``<source>`` is always a dependency of its corresponding copy
     command).

   ``NO_ALL``
     Do not add the generated target as a dependency of the ``all``
     target.

   ``PROGRAMS``
     Copied files should be made executable at their destination.

   ``WORKING_DIRECTORY <dir>``
     Specify the working directory for the copy command (default
     :variable:`CMAKE_CURRENT_BINARY_DIR
     <cmake-ref-current:variable:CMAKE_CURRENT_BINARY_DIR>`).

   .. _cet_copy-single-source-options:

   Single-source options
   """""""""""""""""""""

   ``NAME <name>``
     New name for the file in its final destination.

   ``NAME_AS_TARGET``
     If specified, the target name will be the basename of the
     destination file; otherwise it will be formed by calculating the
     destination path relative to
     :variable:`CETMODULES_CURRENT_PROJECT_BINARY_DIR` and replacing
     path separators with ``_``.

   ``TARGET_VAR <var>``
     Return the calculated target name as ``<var>``.

#]================================================================]

function(cet_copy)
  cmake_parse_arguments(
    PARSE_ARGV 0 CETC "PROGRAMS;NAME_AS_TARGET;NO_ALL"
    "DESTINATION;NAME;TARGET_VAR;WORKING_DIRECTORY" "DEPENDENCIES"
    )
  if(NOT CETC_DESTINATION)
    message(FATAL_ERROR "Missing required option argument DESTINATION")
  endif()
  list(LENGTH CETC_UNPARSED_ARGUMENTS num_sources)
  if(num_sources GREATER 1
     AND (CETC_NAME
          OR CET_NAME_AS_TARGET
          OR CET_TARGET_VAR)
     )
    message(
      STATUS
        "Specification of multiple sources is incompatible with NAME, NAME_AS_TARGET, or TARGET_VAR"
      )
  endif()
  get_filename_component(
    real_dest "${CETC_DESTINATION}" REALPATH BASE_DIR
    "${CMAKE_CURRENT_BINARY_DIR}"
    )
  if(NOT CETC_WORKING_DIRECTORY)
    set(CETC_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  endif()
  foreach(source IN LISTS CETC_UNPARSED_ARGUMENTS)
    if(CETC_NAME)
      set(dest_path "${real_dest}/${CETC_NAME}")
    else()
      get_filename_component(source_base "${source}" NAME)
      set(dest_path "${real_dest}/${source_base}")
    endif()
    if(CETC_NAME_AS_TARGET)
      get_filename_component(target ${dest_path} NAME)
    else()
      cet_package_path(dest_path_target PATH "${dest_path}" TOP_PROJECT BINARY)
      if(dest_path_target)
        string(REPLACE "/" "+" target "${dest_path_target}")
      else()
        string(REPLACE "/" "+" target "${dest_path}")
      endif()
    endif()
    string(REGEX REPLACE "[: ]" "+" target "${target}")
    if(CETC_PROGRAMS)
      set(chmod_cmd COMMAND chmod +x "${dest_path}")
    endif()
    add_custom_command(
      OUTPUT "${dest_path}"
      WORKING_DIRECTORY "${CETC_WORKING_DIRECTORY}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${real_dest}"
      COMMAND ${CMAKE_COMMAND} -E copy "${source}" "${dest_path}" ${chmod_cmd}
      COMMENT "Copying ${source} to ${dest_path}"
      VERBATIM COMMAND_EXPAND_LISTS
      DEPENDS "${source}" ${CETC_DEPENDENCIES}
      )
    if(CETC_NO_ALL)
      set(all_opt)
    else()
      set(all_opt ALL)
    endif()
    add_custom_target(
      ${target}
      ${all_opt}
      DEPENDS "${dest_path}"
      )
    if(CETC_TARGET_VAR)
      set(${CETC_TARGET_VAR}
          "${target}"
          PARENT_SCOPE
          )
    endif()
    set_property(TARGET ${target} PROPERTY CET_EXEC_LOCATION "${dest_path}")
  endforeach()
endfunction()
