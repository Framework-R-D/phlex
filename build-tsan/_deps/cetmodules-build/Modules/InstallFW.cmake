#[================================================================[.rst:
InstallFW
---------

.. admonition:: HEP
   :class: admonition-domain

   Define the function :command:`install_fw` to install HEP framework
   data files.

#]================================================================]

# Avoid unwanted repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

include(CetInstall)
include(ProjectVariable)

#[================================================================[.rst:
.. command:: install_fw

   .. admonition:: HEP
      :class: admonition-app

      Install HEP framework data files in
      :variable:`<PROJECT-NAME>_FW_DIR`.

      .. code-block:: cmake

         install_fw(LIST <file> ... [SUBDIRNAME <subdir>])

   Options
   ^^^^^^^

   .. include:: /_cet_install_opts/LIST.rst

   .. include:: /_cet_install_opts/SUBDIRNAME.rst

#]================================================================]

function(install_fw)
  project_variable(
    FW_DIR
    CONFIG
    NO_WARN_DUPLICATE
    OMIT_IF_EMPTY
    OMIT_IF_MISSING
    OMIT_IF_NULL
    DOCSTRING
    "Directory below prefix to install FW files"
    )
  if(product AND "$CACHE{${product}_fwdir}" MATCHES "^\\\$") # Resolve
                                                             # placeholder.
    set_property(
      CACHE ${product}_fwdir PROPERTY VALUE "${$CACHE{${product}_fwdir}}"
      )
  endif()
  list(REMOVE_ITEM ARGN PROGRAMS) # Not meaningful.
  _cet_install(fw ${CETMODULES_CURRENT_PROJECT_NAME}_FW_DIR ${ARGN} _LIST_ONLY)
endfunction()
