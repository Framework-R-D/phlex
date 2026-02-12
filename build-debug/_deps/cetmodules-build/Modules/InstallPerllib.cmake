#[================================================================[.rst:
InstallPerllib
--------------

.. admonition:: art-suite
   :class: admonition-app

   Install art-suite Perl modules for plugin template generation.

#]================================================================]

# Avoid unwanted repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

include(CetCMakeUtils)
include(CetInstall)
include(CetPackagePath)
include(InstallGdml)
include(ProjectVariable)

#[================================================================[.rst:
.. command:: install_perllib

   .. admonition:: art-suite
      :class: admonition-app

      Install art-suite Perl modules in
      :variable:`<PROJECT-NAME>_PERLLIB_DIR` for plugin template
      generation.

      .. code-block:: cmake

         install_gdml([GLOB] [DROP_PREFIX <dir>] [SUBDIRNAME <subdir>] [<glob-options>])

      .. rst-class:: text-start

      Install recognized files found under
      :variable:`CMAKE_CURRENT_SOURCE_DIR
      <cmake-ref-current:variable:CMAKE_CURRENT_SOURCE_DIR>` or
      :variable:`CMAKE_CURRENT_BINARY_DIR
      <cmake-ref-current:variable:CMAKE_CURRENT_BINARY_DIR>` in
      :variable:`<PROJECT-NAME>_PERLLIB_DIR`.

      Recognized files
        * :file:`*.pm`
        * :file:`*README*`

   Options
   ^^^^^^^

   ``DROP_PREFIX <dir>``
     Remove ``<dir>`` from the beginning of the subdirectory path before
     installation (default
     :variable:`<PROJECT-NAME>_PERLLIB_DIR`).

   .. include:: /_cet_install_opts/SUBDIRNAME.rst

   .. include:: /_cet_install_opts/glob-opts.rst

#]================================================================]

function(install_perllib)
  project_variable(
    PERLLIB_DIR
    perllib
    CONFIG
    NO_WARN_DUPLICATE
    OMIT_IF_EMPTY
    OMIT_IF_MISSING
    OMIT_IF_NULL
    DOCSTRING
    "Directory below prefix to install perl files"
    )
  if(product AND "$CACHE{${product}_perllib}" MATCHES "^\\\$") # Resolve
                                                               # placeholder.
    set_property(
      CACHE ${product}_perllib PROPERTY VALUE "${$CACHE{${product}_perllib}}"
      )
  endif()
  list(REMOVE_ITEM ARGN PROGRAMS) # Not meaningful.
  cmake_parse_arguments(PARSE_ARGV 0 IPRL "" "DROP_PREFIX;SUBDIRNAME" "")
  if(NOT DEFINED IPRL_DROP_PREFIX)
    set(IPRL_DROP_PREFIX ${${CETMODULES_CURRENT_PROJECT_NAME}_PERLLIB_DIR})
  endif()
  cet_package_path(CURRENT_SUBDIR SOURCE BASE_SUBDIR ${IPRL_DROP_PREFIX})
  string(APPEND IPRL_SUBDIRNAME "/${CURRENT_SUBDIR}")
  get_filename_component(CURRENT_SUBDIR_NAME "${CURRENT_SUBDIR}" NAME)
  set(PLUGIN_VERSION_FILE)
  if(CURRENT_SUBDIR_NAME STREQUAL "CetSkelPlugins")
    _cet_perl_plugin_version(PLUGIN_VERSION_FILE)
  endif()
  _cet_install(
    perllib
    ${CETMODULES_CURRENT_PROJECT_NAME}_PERLLIB_DIR
    ${IPRL_UNPARSED_ARGUMENTS}
    SUBDIRNAME
    ${IPRL_SUBDIRNAME}
    _NO_LIST
    _INSTALLED_FILES_VAR
    ${IPRL_INSTALLED_FILES_VAR}
    _EXTRA_EXTRAS
    ${PLUGIN_VERSION_FILE}
    _GLOBS
    "?*.pm"
    "*README*"
    )
  _cet_perllib_config_setup(${_INSTALLED_FILES})
endfunction(install_perllib)

function(_cet_perl_plugin_version PLUGINVERSIONINFO_VAR)
  find_package(cetlib REQUIRED)
  if(cetlib_PLUGINVERSIONINFO_PM_IN)
    cet_localize_pv(cetlib PLUGINVERSIONINFO_PM_IN)
  elseif(cetlib_SOURCE_DIR) # Old cetlib via MRB.
    set(cetlib_PLUGINVERSIONINFO_PM_IN
        "${cetlib_SOURCE_DIR}/perllib/PluginVersionInfo.pm.in"
        )
  else() # Old cetlib installed externally.
    set(cetlib_PLUGINVERSIONINFO_PM_IN
        "$ENV{CETLIB_DIR}/perllib/PluginVersionInfo.pm.in"
        )
  endif()
  set(tmp
      "${CMAKE_CURRENT_BINARY_DIR}/${CETMODULES_CURRENT_PROJECT_NAME}/PluginVersionInfo.pm"
      )
  configure_file("${cetlib_PLUGINVERSIONINFO_PM_IN}" "${tmp}" @ONLY)
  set(${PLUGINVERSIONINFO_VAR}
      "${tmp}"
      PARENT_SCOPE
      )
endfunction()

function(_cet_perllib_config_setup)
  cmake_parse_arguments(PARSE_ARGV 0 _CPCS "PLUGINS" "" "")
  if(_CPCS_PLUGINS)
    set(docstring "Location of cetmod plugin module")
  else()
    set(docstring "Location of cetmod plugin support file")
  endif()
  foreach(pmfile IN LISTS _CPCS_UNPARSED_ARGUMENTS)
    if(_CPCS_PLUGINS)
      get_filename_component(pmvarname "${pmfile}" NAME)
    else()
      set(pmvarname "${pmfile}")
    endif()
    string(REGEX REPLACE [=[[-./ ]]=] "_" pmvarname "${pmvarname}")
    string(TOUPPER "${pmvarname}" pmvarname)
    project_variable(
      "${pmvarname}"
      TYPE
      FILEPATH_FRAGMENT
      CONFIG
      NO_WARN_DUPLICATE
      DOCSTRING
      "${docstring}"
      "${pmfile}"
      )
  endforeach()
endfunction(_cet_perllib_config_setup)
