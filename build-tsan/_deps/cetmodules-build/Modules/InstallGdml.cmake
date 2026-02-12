#[================================================================[.rst:
InstallGdml
-----------

.. admonition:: Geant4
   :class: admonition-app

   Define the function :command:`install_gdml` to install GDML geometry
   description files.

#]================================================================]

# Avoid unwanted repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

include(CetInstall)
include(ProjectVariable)

#[================================================================[.rst:
.. command:: install_gdml

   .. admonition:: HEP
      :class: admonition-app

      Install GDML geometry description files in
      :variable:`<PROJECT-NAME>_GDML_DIR`.

      .. parsed-literal::

         install_gdml(`LIST`_ <file> ... [SUBDIRNAME <subdir>])

      .. parsed-literal::

         install_gdml([`GLOB`_] [SUBDIRNAME <subdir>] [<glob-options>])

   .. signature:: install_gdml(LIST <file> ... [SUBDIRNAME <subdir>])

      Install ``<file> ...`` in :variable:`<PROJECT-NAME>_GDML_DIR`.

      .. include:: /_cet_install_opts/LIST.rst

   .. signature:: install_gdml(GLOB [SUBDIRNAME <subdir>] [<glob-options>])

      .. rst-class:: text-start

      Install recognized files found under
      :variable:`CMAKE_CURRENT_SOURCE_DIR
      <cmake-ref-current:variable:CMAKE_CURRENT_SOURCE_DIR>` or
      :variable:`CMAKE_CURRENT_BINARY_DIR
      <cmake-ref-current:variable:CMAKE_CURRENT_BINARY_DIR>` in
      :variable:`<PROJECT-NAME>_GDML_DIR`.

      Recognized files
        * :file:`*.gdml`

      .. include:: /_cet_install_opts/glob-opts.rst

   Common Options
   ^^^^^^^^^^^^^^

   .. include:: /_cet_install_opts/SUBDIRNAME.rst

#]================================================================]

function(install_gdml)
  project_variable(
    GDML_DIR
    gdml
    CONFIG
    NO_WARN_DUPLICATE
    OMIT_IF_EMPTY
    OMIT_IF_MISSING
    OMIT_IF_NULL
    DOCSTRING
    "Directory below prefix to install GDML geometry description files"
    )
  if(product AND "$CACHE{${product}_gdmldir}" MATCHES "^\\\$") # Resolve
                                                               # placeholder.
    set_property(
      CACHE ${product}_gdmldir PROPERTY VALUE "${$CACHE{${product}_gdmldir}}"
      )
  endif()
  list(REMOVE_ITEM ARGN PROGRAMS) # Not meaningful.
  if("LIST" IN_LIST ARGN)
    _cet_install(gdml ${CETMODULES_CURRENT_PROJECT_NAME}_GDML_DIR ${ARGN})
  else()
    _cet_install(
      gdml
      ${CETMODULES_CURRENT_PROJECT_NAME}_GDML_DIR
      ${ARGN}
      _GLOBS
      "?*.C"
      "?*.gdml"
      "?*.xml"
      "?*.xsd"
      "README"
      )
  endif()
endfunction()
