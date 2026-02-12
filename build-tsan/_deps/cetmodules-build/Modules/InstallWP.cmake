#[================================================================[.rst:
InstallWP
---------

.. admonition:: HEP
   :class: admonition-domain

   Define the function :command:`install_wp` to install HEP framework
   "WP" data files.

#]================================================================]

# Avoid unwanted repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

include(CetInstall)
include(ProjectVariable)

#[================================================================[.rst:
.. command:: install_wp

   .. admonition:: HEP
      :class: admonition-app

      Install HEP framework "WP" data files in
      :variable:`<PROJECT-NAME>_WP_DIR`.

      .. code-block:: cmake

         install_wp(LIST <file> ... [SUBDIRNAME <subdir>])

   Options
   ^^^^^^^

   .. include:: /_cet_install_opts/LIST.rst

   .. include:: /_cet_install_opts/SUBDIRNAME.rst

#]================================================================]

function(install_wp)
  project_variable(
    WP_DIR
    CONFIG
    NO_WARN_DUPLICATE
    OMIT_IF_EMPTY
    OMIT_IF_MISSING
    OMIT_IF_NULL
    DOCSTRING
    "Directory below prefix to install WP files"
    )
  if(product AND "$CACHE{${product}_wpdir}" MATCHES "^\\\$") # Resolve
                                                             # placeholder.
    set_property(
      CACHE ${product}_wpdir PROPERTY VALUE "${$CACHE{${product}_wpdir}}"
      )
  endif()
  list(REMOVE_ITEM ARGN PROGRAMS) # Not meaningful.
  _cet_install(wp ${CETMODULES_CURRENT_PROJECT_NAME}_WP_DIR ${ARGN} _LIST_ONLY)
endfunction()
