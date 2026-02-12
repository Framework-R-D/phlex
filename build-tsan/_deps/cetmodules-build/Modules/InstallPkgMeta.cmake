#[================================================================[.rst:
InstallPkgMeta
--------------

This module defines :command:`install_pkgmeta` and the deprecated
:command:`install_license` to install metadata files such as
``INSTALL``, ``LICENSE``, or ``README``.

#]================================================================]

# Avoid unwanted repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

include(CetInstall)
include(ProjectVariable)

#[================================================================[.rst:
.. command:: install_pkgmeta

   Install metadata files.

   .. code-block:: cmake

      install_pkgmeta([<options>])

   Options
   ^^^^^^^

   ``INSTALLER_LICENSE <license-file>``
     Install the specified ``LICENSE`` file.

   ``INSTALLER_README <readme-file>``
     Install the specified ``README`` file.

   ``INSTALLER_WELCOME <welcome-file>``
     Install the specified ``WELCOME`` file.

   Unrecognized options are passed to :command:`_cet_install`.

   Details
   ^^^^^^^

   The following patterns are used to identify metadata files for
   installation in addition to any options used:

   * ``COPYING``, ``COPYING.*``
   * ``LICEN[CS]E``, ``LICEN[CS]E.*``
   * ``LICEN[CS]ES``, ``LICEN[CS]ES.*``
   * ``INSTALL*``, ``*README*``

   .. rst-class:: text-start

   If not identified specifically by the relevant option, the
   lexically-first identified ``LICENSE``, ``README``, or ``WELCOME``
   files will be used to set :variable:`CPACK_RESOURCE_FILE_LICENSE
   <cmake-ref-current:variable:CPACK_RESOURCE_FILE_LICENSE>`,
   :variable:`CPACK_RESOURCE_FILE_README
   <cmake-ref-current:variable:CPACK_RESOURCE_FILE_README>`, and,
   :variable:`CPACK_RESOURCE_FILE_WELCOME
   <cmake-ref-current:variable:CPACK_RESOURCE_FILE_WELCOME>`
   respectively.

#]================================================================]

function(install_pkgmeta)
  project_variable(
    PKG_META_DIR
    .
    NO_WARN_DUPLICATE
    NO_WARN_REDUNDANT
    OMIT_IF_NULL
    DOCSTRING
    "Directory below prefix to install package metadata such as INSTALL, README and LICENSE files."
    )
  list(REMOVE_ITEM ARGN PROGRAMS) # Not meaningful.
  cmake_parse_arguments(
    PARSE_ARGV 0 _IP "" "INSTALLER_LICENSE;INSTALLER_README;INSTALLER_WELCOME"
    ""
    )
  _cet_install(
    pkgmeta
    ${CETMODULES_CURRENT_PROJECT_NAME}_PKG_META_DIR
    ${_IP_UNPARSED_ARGUMENTS}
    _SQUASH_SUBDIRS
    _INSTALL_ONLY
    _EXTRA_EXTRAS
    ${_IP_INSTALLER_LICENSE}
    ${_IP_INSTALLER_README}
    ${_IP_INSTALLER_WELCOME}
    _INSTALLED_FILES_VAR
    installed_files
    _GLOBS
    "INSTALL*"
    "*README*"
    "LICEN[CS]E"
    "LICEN[CS]E.*"
    "LICEN[CS]ES"
    "LICEN[CS]ES.*"
    "COPYING"
    "COPYING.*"
    )
  if(DEFINED _IP_INSTALLER_LICENSE)
    set(license ${_IP_INSTALLER_LICENSE})
  else()
    set(licenses ${installed_files})
    list(FILTER licenses INCLUDE REGEX [[(^|/)LICEN[CS]ES?(\.[^/]+)?$]])
    if(licenses)
      list(GET licenses 0 license)
    endif()
  endif()
  if(DEFINED license)
    set(${CETMODULES_CURRENT_PROJECT_NAME}_CPACK_RESOURCE_FILE_LICENSE
        "${license}"
        CACHE INTERNAL
              "Installer license file for CMake Project ${CMAKE_PROJECT}"
        )
  endif()
  if(DEFINED _IP_INSTALLER_README)
    set(readme "${_IP_INSTALLER_README}")
  else()
    set(readmes ${installed_files})
    list(FILTER readmes INCLUDE REGEX [=[[^/]*README[^/]*$]=])
    if(readmes)
      list(GET readmes 0 readme)
    endif()
  endif()
  if(DEFINED readme)
    set(${CETMODULES_CURRENT_PROJECT_NAME}_CPACK_RESOURCE_FILE_README
        "${readme}"
        CACHE INTERNAL
              "Installer README file for CMake Project ${CMAKE_PROJECT}"
        )
  endif()
  if(DEFINED _IP_INSTALLER_WELCOME)
    set(${CETMODULES_CURRENT_PROJECT_NAME}_CPACK_RESOURCE_FILE_WELCOME
        "${_IP_INSTALLER_WELCOME}"
        CACHE INTERNAL
              "Installer WELCOME file for CMake Project ${CMAKE_PROJECT}"
        )
  endif()
endfunction()

#[================================================================[.rst:
.. command:: install_license

   Install metadata files.

   ..deprecated:: use :command:`install_pkgmeta`.

#]================================================================]

function(install_license)
  message(
    WARNING "install_license() is deprecated in favor of install_pkgmeta()"
    )
  install_pkgmeta(${ARGN})
endfunction()
