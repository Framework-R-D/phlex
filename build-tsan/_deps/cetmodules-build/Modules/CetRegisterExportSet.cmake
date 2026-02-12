#[================================================================[.rst:
CetRegisterExportSet
--------------------

Defines :command:`cet_register_export_set` to register export sets and
target namespaces.

#]================================================================]

include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

include(ProjectVariable)

#[================================================================[.rst:
.. command:: cet_register_export_set

   Register export sets and target namespaces; also determine the
   default namespace to use for local (non-exported) target aliases.

   .. code-block:: cmake

      cet_register_export_set([<options])

   Options
   ^^^^^^^

   ``NAMESPACE <namespace>``
     Set the namespace to be associated with the export set.

   ``NAMESPACE_VAR <var>``
     Return the namespace in ``<var>``.

   ``NO_REDEFINE``
     Do not attempt to redefine the export set if it exists already.

   ``SET_DEFAULT``
     Set the default export set to be used by Cetmodules build commands
     if not otherwise specified.

   ``SET_NAME <name>``
     Specify the export set name to ``name`` (defaults to
     :variable:`<PROJECT-NAME>_DEFAULT_EXPORT_SET`).

   ``SET_VAR <var>``
     Return the export set name in ``var``.

   Details
   ^^^^^^^

   ``cet_register_export_set`` registers an export set with an
   optionally-defined namespace exported targets, optionally setting it
   as default for use by Cetmodules commands such as
   :command:`cet_make_library` and :command:`cet_make_exec`.

   .. note::

      An export set should be unique to a given project: the
      :variable:`CETMODULES_CURRENT_PROJECT_NAME` is therefore prepended
      to any spacified export set name separated by ``_`` unless it
      already has such a prefix.

   Examples
   ^^^^^^^^

   * .. code-block:: cmake

        cet_register_export_set(NAMESPACE_VAR ns NO_REDEFINE)

     Return the current default namespace scope as variable ``ns``

   * .. code-block:: cmake

        cet_register_export_set(SET_VAR es NO_REDEFINE)

     Return the current default export set name as variable ``es``

   * .. code-block:: cmake

        cet_register_export_set(SET_NAME my_es NAMESPACE my_ns SET_DEFAULT SET_VAR ns)

     Define a new export set ``<PROJECT-NAME>_my_es`` with namespace
     ``my_ns`` and set it as the default. Subsequent targets will be
     exported in this set with exported target name ``my_ns::<target>``
     if not otherwise specified.

   Variables affecting behavior
   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^

   * :variable:`<PROJECT-NAME>_DEFAULT_EXPORT_SET`
   * :variable:`<PROJECT-NAME>_NAMESPACE`

#]================================================================]

function(cet_register_export_set)
  cmake_parse_arguments(
    PARSE_ARGV 0 CRES "NO_REDEFINE;SET_DEFAULT"
    "NAMESPACE;NAMESPACE_VAR;SET_NAME;SET_VAR" ""
    )
  project_variable(
    DEFAULT_EXPORT_SET
    TYPE
    BOOL
    NO_WARN_DUPLICATE
    DOCSTRING
    "\
Default export set to use for targets installed by CET commands. \
Also used for determining namespace for local aliases\
"
    )
  if(CRES_SET_NAME)
    set(EXPORT_SET "${CRES_SET_NAME}")
  else()
    if(CRES_SET_DEFAULT)
      # Reset to original value.
      unset(${CETMODULES_CURRENT_PROJECT_NAME}_DEFAULT_EXPORT_SET)
      unset(${CETMODULES_CURRENT_PROJECT_NAME}_DEFAULT_EXPORT_SET PARENT_SCOPE)
    endif()
    set(EXPORT_SET "${${CETMODULES_CURRENT_PROJECT_NAME}_DEFAULT_EXPORT_SET}")
  endif()
  if(NOT EXPORT_SET MATCHES "^${CETMODULES_CURRENT_PROJECT_NAME}")
    string(PREPEND EXPORT_SET "${CETMODULES_CURRENT_PROJECT_NAME}")
  endif()
  if(NOT CRES_SET_NAME)
    message(VERBOSE "unspecified export set defaults to ${EXPORT_SET}")
  elseif(NOT CRES_SET_NAME STREQUAL EXPORT_SET)
    message(VERBOSE
            "export set name ${CRES_SET_NAME} -> ${EXPORT_SET} to avoid clashes"
            )
  endif()
  if(NOT "${EXPORT_SET}" IN_LIST
     CETMODULES_EXPORT_SETS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
     )
    if(NOT DEFINED
       CACHE{CETMODULES_EXPORT_SETS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}}
       )
      set(CETMODULES_EXPORT_SETS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
          "${EXPORT_SET}"
          CACHE INTERNAL
                "List of export sets for ${CETMODULES_CURRENT_PROJECT_NAME}"
          )
    else()
      set_property(
        CACHE CETMODULES_EXPORT_SETS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
        APPEND
        PROPERTY VALUE "${EXPORT_SET}"
        )
    endif()
    if(NOT CRES_NAMESPACE)
      set(CRES_NAMESPACE ${${CETMODULES_CURRENT_PROJECT_NAME}_NAMESPACE})
    endif()
    if(NOT CRES_NAMESPACE)
      string(TOLOWER "${CETMODULES_CURRENT_PROJECT_NAME}" CRES_NAMESPACE)
      string(REPLACE "-" "_" CRES_NAMESPACE "${CRES_NAMESPACE}")
    endif()
    if(CRES_NAMESPACE MATCHES "^(.*)::\$")
      set(CRES_NAMESPACE "${CMAKE_MATCH_1}")
    endif()
    message(VERBOSE
            "export set ${EXPORT_SET} mapped to namespace ${CRES_NAMESPACE}"
            )
    set(CETMODULES_NAMESPACE_EXPORT_SET_${EXPORT_SET}_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
        ${CRES_NAMESPACE}
        CACHE
          INTERNAL
          "Namespace for export set ${EXPORT_SET} of project ${CETMODULES_CURRENT_PROJECT_NAME}"
        )
  elseif(NOT CRES_NO_REDEFINE)
    if(CRES_NAMESPACE)
      message(
        WARNING
          "attempt to set namespace for existing export set ${EXPORT_SET} (currently \"${CETMODULES_NAMESPACE_EXPORT_SET_${EXPORT_SET}_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}}\") ignored"
        )
    endif()
    message(
      VERBOSE
      "Lowering the dependency precedence of existing export set ${EXPORT_SET}"
      )
    set_property(
      CACHE CETMODULES_EXPORT_SETS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
      APPEND
      PROPERTY VALUE "${EXPORT_SET}"
      )
  endif()
  if(CRES_SET_VAR)
    set(${CRES_SET_VAR}
        ${EXPORT_SET}
        PARENT_SCOPE
        )
  endif()
  if(CRES_NAMESPACE_VAR)
    set(${CRES_NAMESPACE_VAR}
        ${CETMODULES_NAMESPACE_EXPORT_SET_${EXPORT_SET}_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}}
        PARENT_SCOPE
        )
  endif()
  if(CRES_SET_DEFAULT AND CRES_SET_NAME)
    set(${CETMODULES_CURRENT_PROJECT_NAME}_DEFAULT_EXPORT_SET
        ${EXPORT_SET}
        PARENT_SCOPE
        )
  endif()
endfunction()
