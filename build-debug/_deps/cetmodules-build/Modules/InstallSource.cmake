#[================================================================[.rst:
InstallSource
-------------

Install source files.

#]================================================================]

# ##############################################################################
# install_source()
#
# Install source files under
# ${${CETMODULES_CURRENT_PROJECT_NAME}_INSTALLED_SOURCE_DIR}
#
# Usage: install_source([SUBDIRNAME <subdir>] LIST ...)
# install_source([SUBDIRNAME <subdir>] [BASENAME_EXCLUDES ...] [EXCLUDES ...]
# [EXTRAS ...] [SUBDIRS ...])
#
# See CetInstall.cmake for full usage description.
#
# Recognized filename extensions: .h .hh .H .hpp .hxx .icc .tcc .c .cc .cpp .C
# .cxx .sh .py .pl .rb .xml .dox
#
# Other recognized patterns: INSTALL* *README* LICENSE LICENSE.* COPYING
# COPYING.*
#
# Excluded files: ?*.bak ?*.~ ?*.~[0-9]* ?*.old ?*.orig ?*.rej #*# .DS_Store
#
# ##############################################################################

# Avoid unwanted repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

include(CetInstall)
include(CetPackagePath)
include(ProjectVariable)

#[================================================================[.rst:
.. command:: install_source

   Install sources in :variable:`<PROJECT-NAME>_INSTALLED_SOURCE_DIR`.

   .. parsed-literal::

      install_source(`LIST`_ <file> ... [<common-options>])

   .. parsed-literal::

      install_source([`GLOB`_] [<common-options>] [<glob-options>])

   .. signature:: install_source(LIST <file> ... [<options>]

      Install ``<file> ...`` in
      :variable:`<PROJECT-NAME>_INSTALLED_SOURCE_DIR`.

      .. include:: /_cet_install_opts/LIST.rst

   .. signature:: install_source(GLOB [<common-options>] [<glob-options>])

      .. rst-class:: text-start

      Install recognized files found under
      :variable:`CMAKE_CURRENT_SOURCE_DIR
      <cmake-ref-current:variable:CMAKE_CURRENT_SOURCE_DIR>` or
      :variable:`CMAKE_CURRENT_BINARY_DIR
      <cmake-ref-current:variable:CMAKE_CURRENT_BINARY_DIR>` in
      :variable:`<PROJECT-NAME>_INCLUDE_DIR`.

      Recognized files
        .. hlist::
           :columns: 6

           * :file:`*.C`
           * :file:`*.H`
           * :file:`*.c`
           * :file:`*.cc`
           * :file:`*.cpp`
           * :file:`*.cxx`
           * :file:`*.dox`
           * :file:`*.h`
           * :file:`*.hh`
           * :file:`*.hpp`
           * :file:`*.hxx`
           * :file:`*.icc`
           * :file:`*.pl`
           * :file:`*.py`
           * :file:`*.rb`
           * :file:`*.sh`
           * :file:`*.tcc`
           * :file:`*.xml`
           * :file:`*README*`
           * :file:`COPYING.*`
           * :file:`COPYING`
           * :file:`INSTALL*`
           * :file:`LICENSE.*`
           * :file:`LICENSE`

      Excluded files
        .. hlist::
           :columns: 4

           * :file:`*.bak`
           * :file:`*.~`
           * :file:`*.~[0-9]*`
           * :file:`*.old`
           * :file:`*.orig`
           * :file:`*.rej`
           * :file:`#*#`
           * :file:`.DS_Store`

      .. include:: /_cet_install_opts/glob-opts.rst

   Common Options
   ^^^^^^^^^^^^^^

   .. include:: /_cet_install_opts/SUBDIRNAME.rst

#]================================================================]

function(install_source)
  project_variable(
    INSTALLED_SOURCE_DIR
    "source"
    CONFIG
    NO_WARN_DUPLICATE
    OMIT_IF_EMPTY
    OMIT_IF_MISSING
    OMIT_IF_NULL
    DOCSTRING
    "Directory below prefix to install source files for debug and other purposes"
    )
  list(REMOVE_ITEM ARGN PROGRAMS) # Not meaningful.
  cmake_parse_arguments(PARSE_ARGV 0 IS "" "SUBDIRNAME" "")
  cet_package_path(CURRENT_SUBDIR)
  string(APPEND IS_SUBDIRNAME "/${CURRENT_SUBDIR}")
  if("LIST" IN_LIST IS_UNPARSED_ARGUMENTS)
    _cet_install(
      source ${CETMODULES_CURRENT_PROJECT_NAME}_INSTALLED_SOURCE_DIR
      ${IS_UNPARSED_ARGMUENTS} SUBDIRNAME ${IS_SUBDIRNAME} _INSTALL_ONLY
      )
  else()
    _cet_install(
      source
      ${CETMODULES_CURRENT_PROJECT_NAME}_INSTALLED_SOURCE_DIR
      ${IS_UNPARSED_ARGUMENTS}
      SUBDIRNAME
      ${IS_SUBDIRNAME}
      _SEARCH_BUILD
      _INSTALL_ONLY
      _EXTRA_BASENAME_EXCLUDES
      "?*.bak"
      "?*.~"
      "?*.~[0-9]*"
      "?*.old"
      "?*.orig"
      "?*.rej"
      "#*#"
      ".DS_Store"
      _GLOBS
      "?*.h"
      "?*.hh"
      "?*.H"
      "?*.hpp"
      "?*.hxx"
      "?*.icc"
      "?*.tcc"
      "?*.c"
      "?*.cc"
      "?*.C"
      "?*.cpp"
      "?*.cxx"
      "?*.sh"
      "?*.py"
      "?*.pl"
      "?*.rb"
      "?*.xml"
      "?*.dox"
      "INSTALL*"
      "*README*"
      "LICENSE"
      "LICENSE.*"
      "COPYING"
      "COPYING.*"
      )
  endif()
endfunction()
