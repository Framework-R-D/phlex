#[================================================================[.rst:
InstallScripts
--------------

Install scripts.

#]================================================================]

# Avoid unwanted repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

include(CetInstall)
include(ProjectVariable)

#[================================================================[.rst:
.. command:: install_scripts

   Install scripts in :variable:`<PROJECT-NAME>_SCRIPTS_DIR` or
   :variable:`<PROJECT-NAME>_TEST_DIR` (with ``AS_TEST``).

   .. parsed-literal::

      install_scripts(`LIST`_ <file> ... [<common-options>])

   .. parsed-literal::

      install_scripts([`GLOB`_] [<common-options>] [<glob-options>])

   .. signature:: install_scripts(LIST <file> ... [<options>]

      Install ``<file> ...`` in :variable:`<PROJECT-NAME>_SCRIPTS_DIR`
      or :variable:`<PROJECT-NAME>_TEST_DIR` (with ``AS_TEST``).

      .. include:: /_cet_install_opts/LIST.rst

   .. signature:: install_scripts(GLOB [<common-options>] [<glob-options>])

      .. rst-class:: text-start

      Install recognized files found under
      :variable:`CMAKE_CURRENT_SOURCE_DIR
      <cmake-ref-current:variable:CMAKE_CURRENT_SOURCE_DIR>` or
      :variable:`CMAKE_CURRENT_BINARY_DIR
      <cmake-ref-current:variable:CMAKE_CURRENT_BINARY_DIR>` in
      :variable:`<PROJECT-NAME>_INCLUDE_DIR`.

      Recognized files
        * :file:`*.cfg` (``AS_TEST`` only)
        * :file:`*.pl`
        * :file:`*.py`
        * :file:`*.rb`
        * :file:`*.sh`

      .. include:: /_cet_install_opts/glob-opts.rst

   Common Options
   ^^^^^^^^^^^^^^

   ``AS_TEST``
     Install scripts in :variable:`<PROJECT-NAME>_TEST_DIR` (default
     :variable:`<PROJECT-NAME>_SCRIPTS_DIR`).

   .. include:: /_cet_install_opts/SUBDIRNAME.rst

#]================================================================]

function(install_scripts)
  cmake_parse_arguments(PARSE_ARGV 0 IS "AS_TEST" "DEST_VAR" "")
  set(GLOBS "?*.sh" "?*.py" "?*.pl" "?*.rb")
  list(REMOVE_ITEM IS_UNPARSED_ARGUMENTS PROGRAMS) # Avoid duplication.
  if(IS_AS_TEST)
    if(DEFINED IS_DEST_VAR OR DEST_VAR IN_LIST IS_KEYWORDS_MISSING_VALUES)
      message(FATAL_ERROR "AS_TEST is incompatible with DEST_VAR")
    endif()
    set(IS_DEST_VAR TEST_DIR)
  elseif(NOT DEFINED IS_DEST_VAR)
    set(IS_DEST_VAR SCRIPTS_DIR)
  endif()
  if("LIST" IN_LIST IS_UNPARSED_ARGUMENTS)
    _cet_install(
      scripts ${CETMODULES_CURRENT_PROJECT_NAME}_${IS_DEST_VAR}
      ${IS_UNPARSED_ARGUMENTS} PROGRAMS _INSTALL_ONLY
      )
  else()
    _cet_install(
      scripts
      ${CETMODULES_CURRENT_PROJECT_NAME}_${IS_DEST_VAR}
      ${IS_UNPARSED_ARGUMENTS}
      PROGRAMS
      _INSTALL_ONLY
      _SQUASH_SUBDIRS
      _GLOBS
      ${GLOBS}
      )
    if(IS_AS_TEST)
      # Don't force installed .cfg files to be executable.
      _cet_install(
        scripts
        ${CETMODULES_CURRENT_PROJECT_NAME}_${IS_DEST_VAR}
        ${IS_UNPARSED_ARGUMENTS}
        _INSTALL_ONLY
        _SQUASH_SUBDIRS
        _GLOBS
        "?*.cfg"
        )
    endif()
  endif()
endfunction()
