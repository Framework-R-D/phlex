#[================================================================[.rst:
InstallFhicl
------------

.. admonition:: art-suite
   :class: admonition-app

   Define the function :command:`install_fhicl` to install FHiCL
   configuration files.

#]================================================================]

# Avoid unwanted repeat inclusion.
include_guard()

include(CetInstall)
include(ProjectVariable)

#[================================================================[.rst:
.. command:: install_fhicl

   .. admonition:: HEP
      :class: admonition-app

      Install FHiCL configuration files in
      :variable:`<PROJECT-NAME>_FHICL_DIR`.

      .. parsed-literal::

         install_fhicl(`LIST`_ <file> ... [SUBDIRNAME <subdir>])

      .. parsed-literal::

         install_fhicl([`GLOB`_] [SUBDIRNAME <subdir>] [<glob-options>])

   .. signature:: install_fhicl(LIST <file> ... [SUBDIRNAME <subdir>])

      Install ``<file> ...`` in :variable:`<PROJECT-NAME>_FHICL_DIR`.

      .. include:: /_cet_install_opts/LIST.rst

   .. signature:: install_fhicl(GLOB [SUBDIRNAME <subdir>] [<glob-options>])

      .. rst-class:: text-start

      Install recognized files found under
      :variable:`CMAKE_CURRENT_SOURCE_DIR
      <cmake-ref-current:variable:CMAKE_CURRENT_SOURCE_DIR>` or
      :variable:`CMAKE_CURRENT_BINARY_DIR
      <cmake-ref-current:variable:CMAKE_CURRENT_BINARY_DIR>` in
      :variable:`<PROJECT-NAME>_FHICL_DIR`.

      Recognized files
        * :file:`*.fcl`

      .. include:: /_cet_install_opts/glob-opts.rst

   Common Options
   ^^^^^^^^^^^^^^

   .. include:: /_cet_install_opts/SUBDIRNAME.rst

#]================================================================]

function(install_fhicl)
  project_variable(
    FHICL_DIR
    "fcl"
    CONFIG
    NO_WARN_DUPLICATE
    OMIT_IF_EMPTY
    OMIT_IF_MISSING
    OMIT_IF_NULL
    DOCSTRING
    "Directory below prefix to install FHiCL files"
    )
  if(product AND "$CACHE{${product}_fcldir}" MATCHES "^\\\$") # Resolve
                                                              # placeholder.
    set_property(
      CACHE ${product}_fcldir PROPERTY VALUE "${$CACHE{${product}_fcldir}}"
      )
  endif()
  list(REMOVE_ITEM ARGN PROGRAMS) # Not meaningful.
  if("LIST" IN_LIST ARGN)
    _cet_install(fhicl ${CETMODULES_CURRENT_PROJECT_NAME}_FHICL_DIR ${ARGN})
  else()
    _cet_install(
      fhicl ${CETMODULES_CURRENT_PROJECT_NAME}_FHICL_DIR ${ARGN}
      _SQUASH_SUBDIRS _GLOBS "?*.fcl"
      )
  endif()
endfunction()
