#[================================================================[.rst:
InstallHeaders
--------------

Install headers.

#]================================================================]

# Avoid unwanted repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

include(CetCMakeUtils)
include(CetInstall)
include(CetPackagePath)

#[================================================================[.rst:
.. command:: install_headers

      Install headers in :variable:`<PROJECT-NAME>_INCLUDE_DIR`.

      .. parsed-literal::

         install_headers(`LIST`_ <file> ... [<common-options>])

      .. parsed-literal::

         install_headers([`GLOB`_] [<common-options>] [<glob-options>])

   .. signature:: install_headers(LIST <file> ... [<options>]

      Install ``<file> ...`` in :variable:`<PROJECT-NAME>_INCLUDE_DIR`.

      .. include:: /_cet_install_opts/LIST.rst

   .. signature:: install_headers(GLOB [<common-options>] [<glob-options>])

      .. rst-class:: text-start

      Install recognized files found under
      :variable:`CMAKE_CURRENT_SOURCE_DIR
      <cmake-ref-current:variable:CMAKE_CURRENT_SOURCE_DIR>` or
      :variable:`CMAKE_CURRENT_BINARY_DIR
      <cmake-ref-current:variable:CMAKE_CURRENT_BINARY_DIR>` in
      :variable:`<PROJECT-NAME>_INCLUDE_DIR`.

      Recognized files
        * :file:`*.h`
        * :file:`*.hh`
        * :file:`*.H`
        * :file:`*.hpp`
        * :file:`*.hxx`
        * :file:`*.icc`
        * :file:`*.tcc`

      .. include:: /_cet_install_opts/glob-note.rst

      .. include:: /_cet_install_opts/BASENAME_EXCLUDES.rst

      .. include:: /_cet_install_opts/EXCLUDES.rst

      .. include:: /_cet_install_opts/EXTRAS.rst

      .. include:: /_cet_install_opts/SQUASH_SUBDIRS.rst

      .. include:: /_cet_install_opts/SUBDIRS.rst

   Common Options
   ^^^^^^^^^^^^^^

   ``NO_RELATIVE_SUBDIR``
     Refrain from using the current source directory relative to the
     top-level project directory as the base destination path.

   .. include:: /_cet_install_opts/SUBDIRNAME.rst

   ``USE_PRODUCT_NAME``
     .. deprecated:: 2.10.00

   ``USE_PROJECT_NAME``
     Prepend :variable:`CETMODULES_CURRENT_PROJECT_NAME` to the
     calculated destination path.

#]================================================================]

function(install_headers)
  list(REMOVE_ITEM ARGN PROGRAMS) # Not meaningful.
  cmake_parse_arguments(
    PARSE_ARGV 0 IHDR
    "NO_RELATIVE_SUBDIR;SQUASH_SUBDIRS;USE_PRODUCT_NAME;USE_PROJECT_NAME"
    "SUBDIRNAME" ""
    )
  cet_package_path(CURRENT_SUBDIR)
  if(IHDR_USE_PROJECT_NAME OR IHDR_USE_PRODUCT_NAME)
    string(JOIN "/" IHDR_SUBDIRNAME "${IHDR_SUBDIRNAME}"
           "${CETMODULES_CURRENT_PROJECT_NAME}"
           )
  endif()
  if(NOT IHDR_NO_RELATIVE_SUBDIR)
    string(JOIN "/" IHDR_SUBDIRNAME "${IHDR_SUBDIRNAME}" "${CURRENT_SUBDIR}")
  endif()
  if("LIST" IN_LIST IHDR_UNPARSED_ARGUMENTS)
    _cet_install(
      headers ${CETMODULES_CURRENT_PROJECT_NAME}_INCLUDE_DIR
      ${IHDR_UNPARSED_ARGUMENTS} SUBDIRNAME ${IHDR_SUBDIRNAME} _INSTALL_ONLY
      )
  else()
    cet_passthrough(FLAG IN_PLACE KEYWORD _SQUASH_SUBDIRS IHDR_SQUASH_SUBDIRS)
    _cet_install(
      headers
      ${CETMODULES_CURRENT_PROJECT_NAME}_INCLUDE_DIR
      ${IHDR_UNPARSED_ARGUMENTS}
      SUBDIRNAME
      ${IHDR_SUBDIRNAME}
      _INSTALL_ONLY
      ${IHDR_SQUASH_SUBDIRS}
      _SEARCH_BUILD
      _EXTRA_BASENAME_EXCLUDES
      classes.h
      Linkdef.h
      _GLOBS
      "?*.h"
      "?*.hh"
      "?*.H"
      "?*.hpp"
      "?*.hxx"
      "?*.icc"
      "?*.tcc"
      )
  endif()
endfunction()
