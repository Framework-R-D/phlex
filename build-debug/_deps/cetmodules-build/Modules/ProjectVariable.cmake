#[================================================================[.rst:
ProjectVariable
---------------

Utilities for defining and manipulating project-specific cache
variables.

Defined functions:

:command:`project_variable`
  Define a :manual:`project variable <cetmodules-project-variables(7)>`:
  a project-specific cache variable with optional propagation to
  dependent packages via :command:`find_package()
  <cmake-ref-current:command:find_package>`.

:command:`cet_get_pv_property`
  Get the value of a :manual:`property
  <cmake-ref-current:manual:cmake-properties(7)>`\ -like attribute
  attached to a particular :manual:`project variable
  <cetmodules-project-variables(7)>`.

:command:`cet_set_pv_property`
  Set or add to a :manual:`property
  <cmake-ref-current:manual:cmake-properties(7)>`\ -like attribute
  attached to a particular :manual:`project variable
  <cetmodules-project-variables(7)>`.

.. seealso::

   :command:`cet_localize_pv`, :command:`cet_localize_pv_all`,
   :command:`cet_cmake_config`.

#]================================================================]

# Avoid unwanted repeat inclusion.
include_guard()

# Non-disruptive CMake version requirements.
cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

# Default architecture-specific install directory types.
set(CETMODULES_DEFAULT_ARCH_DIRS BIN_DIR CONFIG_OUTPUT_ROOT_DIR LIBEXEC_DIR
                                 LIBRARY_DIR
    )

# Type classifiers, including two special ones of our own that CMake doesn't
# know about.
set(_CPV_SPECIAL_PATH_TYPES PATH_FRAGMENT FILEPATH_FRAGMENT)
set(_CPV_PATH_TYPES PATH FILEPATH ${_CPV_SPECIAL_PATH_TYPES})

# Names that will clash with CMake's predefined variables.
set(_CPV_RESERVED_NAMES
    BINARY_DIR
    DESCRIPTION
    HOMEPAGE_URL
    SOURCE_DIR
    VERSION
    VERSION_MAJOR
    VERSION_MINOR
    VERSION_PATCH
    VERSION_TWEAK
    )

# Flag properties.
set(_CPV_FLAGS CONFIG IS_PATH MISSING_OK OMIT_IF_EMPTY OMIT_IF_MISSING
               OMIT_IF_NULL
    )

# Option properties.
set(_CPV_OPTIONS ORIGIN TYPE)

#[================================================================[.rst:
.. command:: cet_get_pv_property

   Get the value of a :manual:`property
   <cmake-ref-current:manual:cmake-properties(7)>`\ -like attribute
   attached to a particular :manual:`project variable
   <cetmodules-project-variables(7)>`.

   .. code-block:: cmake

      cet_get_pv_property([<out-var>] [PROJECT] <project-name> <var-name> PROPERTY <property>)

   Options
   ^^^^^^^

   ``[PROJECT] <project-name>``
     Find project variable ``<var-name>`` in project ``<project-name>``
     (default :variable:`${CETMODULES_CURRENT_PROJECT_NAME}
     <CETMODULES_CURRENT_PROJECT_NAME>`). If ``<out-var>`` is specified,
     the ``PROJECT`` keyword is optional.

   ``PROPERTY <property>``
     The property-like attribute of project variable ``<var-name>``
     whose value(s) should be returned in ``<out-var>``.

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``[<out-var>]``
     The variable in which the value(s) of project variable
     ``<var-name>`` in project ``<project-name>`` shall be returned. If
     not specified, the property's values shall be returned in a
     variable in caller's scope whose name is ``<property>``.

#]================================================================]

function(cet_get_pv_property)
  cmake_parse_arguments(PARSE_ARGV 0 GPVP "" "PROJECT" "")
  # Read backwards.
  list(POP_BACK GPVP_UNPARSED_ARGUMENTS PROP PROP_KW VAR_NAME)
  if(GPVP_PROJECT)
    set(PROJ "${GPVP_PROJECT}")
    list(POP_FRONT GPVP_UNPARSED_ARGUMENTS OUT_VAR)
  else()
    list(POP_FRONT GPVP_UNPARSED_ARGUMENTS OUT_VAR PROJ)
    if(NOT PROJ)
      set(PROJ "${CETMODULES_CURRENT_PROJECT_NAME}")
    endif()
  endif()
  if(NOT OUT_VAR)
    set(OUT_VAR "${PROP}")
  endif()
  if(GPVP_UNPARSED_ARGUMENTS
     OR NOT
        (PROJ
         AND VAR_NAME
         AND OUT_VAR
         AND PROP_KW STREQUAL "PROPERTY"
         AND PROP)
     )
    message(
      FATAL_ERROR
        [=[
cet_get_pv_property bad arguments ${ARGV}
USAGE: cet_get_pv_property([<output-variable>] [PROJECT <project-name>] <var-name> PROPERTY <property>)
       If <output-variable> and <project-variable> are both specified, the PROJECT keyword is optional]=]
      )
  endif()
  set(RESULT)
  if(DEFINED CACHE{CETMODULES_${VAR_NAME}_PROPERTIES_PROJECT_${PROJ}})
    get_property(
      cached_properties
      CACHE CETMODULES_${VAR_NAME}_PROPERTIES_PROJECT_${PROJ}
      PROPERTY VALUE
      )
    if(PROP IN_LIST _CPV_FLAGS)
      # Flag.
      if(PROP IN_LIST cached_properties)
        set(RESULT TRUE)
      else()
        set(RESULT FALSE)
      endif()
    else()
      set(RESULT
          $CACHE{CETMODULES_${VAR_NAME}_PROPERTY_${PROP}_PROJECT_${PROJ}}
          )
    endif()
  endif()
  set(${OUT_VAR}
      ${RESULT}
      PARENT_SCOPE
      )
endfunction()

#[================================================================[.rst:
.. command:: cet_set_pv_property

   Set or append values to a :manual:`property
   <cmake-ref-current:manual:cmake-properties(7)>`\ -like attribute
   attached to a particular :manual:`project variable
   <cetmodules-project-variables(7)>`.

   .. code-block:: cmake

      cet_set_pv_property([<project-name>] <var-name> [APPEND|APPEND_STRING] PROPERTY <property> [<value> ...])

   Options
   ^^^^^^^

   ``PROPERTY <property>``
     The property-like attribute of project variable ``<var-name>``
     whose value(s) should be returned in ``<out-var>``.

   ``APPEND``
     ``<property>`` shall be treated as a :command:`list
     <cmake-ref-current:command:list>` for the purposes of appending
     ``<value> ...``.

   ``APPEND_STRING``
     ``<property>`` shall be treated as a :command:`string
     <cmake-ref-current:command:string>` for the purposes of appending
     ``<value> ...``.

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<project-name>``
     Find project variable ``<var-name>`` in project ``<project-name>``
     (default :variable:`${CETMODULES_CURRENT_PROJECT_NAME}
     <CETMODULES_CURRENT_PROJECT_NAME>`).

   ``<value> ...``
     The item(s) to be set for or appended to ``<property>``.

#]================================================================]

function(cet_set_pv_property)
  list(FIND ARGV "PROPERTY" PROP_IDX)
  list(SUBLIST ARGV ${PROP_IDX} -1 VALS)
  list(POP_FRONT VALS PROP_KW PROP)
  list(SUBLIST ARGV 0 ${PROP_IDX} ARGS)
  cmake_parse_arguments(SPVP "APPEND;APPEND_STRING" "" "" ${ARGS})
  list(POP_BACK SPVP_UNPARSED_ARGUMENTS VAR_NAME PROJ)
  if(NOT PROJ)
    set(PROJ "${CETMODULES_CURRENT_PROJECT_NAME}")
  endif()
  if(SPVP_UNPARSED_ARGUMENTS
     OR NOT
        (PROJ
         AND VAR_NAME
         AND PROP_KW STREQUAL "PROPERTY"
         AND PROP)
     )
    message(
      FATAL_ERROR
        "cet_set_pv_property(): bad arguments ${ARGV}"
        "\nUSAGE: cet_set_pv_property([<project-name>] <var-name> [APPEND|APPEND_STRING] PROPERTY <property> [<value>...])"
      )
  endif()
  if(NOT DEFINED CACHE{CETMODULES_${VAR_NAME}_PROPERTIES_PROJECT_${PROJ}})
    set(CETMODULES_${VAR_NAME}_PROPERTIES_PROJECT_${PROJ}
        CACHE INTERNAL "Properties for project variable ${PROJ}_${VAR_NAME}"
        )
  endif()
  get_property(
    cached_properties
    CACHE CETMODULES_${VAR_NAME}_PROPERTIES_PROJECT_${PROJ}
    PROPERTY VALUE
    )
  if(PROP IN_LIST _CPV_FLAGS)
    # Flag.
    if(SPVP_APPEND OR SPVP_APPEND_STRING)
      message(
        FATAL_ERROR
          "cet_set_pv_property(): APPEND/APPEND_STRING invalid for flag ${PROP}"
          "\nSet flag with non-empty <VALUE> evaluating to TRUE, or reset."
        )
    endif()
    list(REMOVE_ITEM cached_properties "${PROP}")
    if(VALS) # Flag should be set.
      list(APPEND cached_properties "${PROP}")
    endif()
  else()
    list(LENGTH VALS nvals)
    if(NOT nvals)
      list(REMOVE_ITEM cached_properties "${PROP}")
      unset(CETMODULES_${VAR_NAME}_PROPERTY_${PROP}_PROJECT_${PROJ} CACHE)
      unset(CETMODULES_${VAR_NAME}_PROPERTY_${PROP}_PROJECT_${PROJ}
            PARENT_SCOPE
            )
    else()
      if(NOT DEFINED
         CACHE{CETMODULES_${VAR_NAME}_PROPERTY_${PROP}_PROJECT_${PROJ}}
         )
        set(CETMODULES_${VAR_NAME}_PROPERTY_${PROP}_PROJECT_${PROJ}
            ${VALS}
            CACHE INTERNAL
                  "Valued property for project variable ${PROJ}_${VAR_NAME}"
            )
      else()
        if(SPVP_APPEND_STRING)
          set(append_kw APPEND_STRING)
        elseif(SPVP_APPEND)
          set(append_kw APPEND)
        else()
          set(append_kw)
        endif()
        set_property(
          CACHE CETMODULES_${VAR_NAME}_PROPERTY_${PROP}_PROJECT_${PROJ}
                ${append_kw} PROPERTY VALUE ${VALS}
          )
      endif()
      list(APPEND cached_properties ${PROP})
    endif()
  endif()
  set_property(
    CACHE CETMODULES_${VAR_NAME}_PROPERTIES_PROJECT_${PROJ}
    PROPERTY VALUE ${cached_properties}
    )
endfunction()

#[================================================================[.rst:
.. command:: project_variable

   Define a project-specific cache variable
   :variable:`${CETMODULES_CURRENT_PROJECT_NAME}
   <CETMODULES_CURRENT_PROJECT_NAME>`\ ``_<var-name>``, extending the
   functionality of :command:`set(... CACHE ...)
   <cmake-ref-current:command:set(cache)>`. Optionally ensure that the
   variable is appropriately defined for dependents via
   :command:`find_package() <cmake-ref-current:command:find_package>`.

   .. seealso:: :manual:`cetmodules-project-variables(7)`.

   .. code-block:: cmake

      project_variable(<var-name> [<options>] [<init-val> ...])

   Options
   ^^^^^^^

   .. _project_variable-BACKUP_DEFAULT:

   ``BACKUP_DEFAULT <default-val> ...``
     If ``<init-val>`` evaluates to ``FALSE`` then ``<default-val> ...``
     will be the initial value of the cached variable.

   .. _project_variable-CONFIG:

   ``CONFIG``
     Add the defined project variable to
     :variable:`${CETMODULES_CURRENT_PROJECT_NAME}
     <CETMODULES_CURRENT_PROJECT_NAME>`\ :file:`Config.cmake` for
     propagation to dependent packages—in a location-independent way if
     appropriate to the variable's `TYPE <project_variable-TYPE>`_\ —via
     :command:`find_package() <cmake-ref-current:command:find_package>`.

   ``DOCSTRING <docstring>``
     A string describing the variable (defaults to a generic
     description).

   .. _project_variable-MISSING_OK:

   ``MISSING_OK``
     A project variable whose `TYPE <project_variable-TYPE>`_ matches
     ``^(FILE)?PATH(_FRAGMENT)?$`` but whose value does not represent a
     valid path in the installation area will cause an error at
     :command:`find_package()
     <cmake-ref-current:command:find_package>`\ -time unless this option
     is specified.

   ``NO_WARN_DUPLICATE``
     Do not warn about multiple attempts to define the same project
     variable in the same project.

   ``NO_WARN_REDUNDANT``
     Do not warn about redundant options (e.g. if ``CONFIG`` is not
     specified).

   .. _project_variable-OMIT_IF_EMPTY:

   ``OMIT_IF_EMPTY``
     .. rst-class:: text-start

     If specified, a project variable representing a directory
     (i.e. whose `TYPE <project_variable-TYPE>`_ matches
     ``^PATH(_FRAGMENT)?$``) will be omitted from
     :variable:`${CETMODULES_CURRENT_PROJECT_NAME}
     <CETMODULES_CURRENT_PROJECT_NAME>`\ :file:`Config.cmake` if it
     contains no entries other than :file:`.` and :file:`..`

   .. _project_variable-OMIT_IF_MISSING:

   ``OMIT_IF_MISSING``
     If specified, a project variable whose `TYPE
     <project_variable-TYPE>`_ matches ``^(FILE)?PATH(_FRAGMENT)?$`` but
     whose value does not represent an existing file or directory in the
     installation area will be omitted from
     :variable:`${CETMODULES_CURRENT_PROJECT_NAME}
     <CETMODULES_CURRENT_PROJECT_NAME>`\ :file:`Config.cmake`.

   .. _project_variable-OMIT_IF_NULL:

   ``OMIT_IF_NULL``
     If specified, the definition of a vacuous project variable will be
     omitted from
     :variable:`${CETMODULES_CURRENT_PROJECT_NAME}
     <CETMODULES_CURRENT_PROJECT_NAME>`\ :file:`Config.cmake`.

   ``PUBLIC``
     Project variables will generally be marked "advanced"—not visible
     by default in CMake configuration GUIs or to
     :external+cmake-ref-current:option:`cmake -N <cmake.-N>`
     :external+cmake-ref-current:option:`-L <cmake.-L[A][H]>` in the
     absence of the ``-A`` option. Specifying ``PUBLIC`` will negate
     this.

     .. seealso::

        :command:`mark_as_advanced() <cmake-ref-current:command:mark_as_advanced>`.

   .. _project_variable-TYPE:

   ``TYPE <type>``

     Define the :ref:`project-variables-types` type of the project variable
     (default ``PATH_FRAGMENT``).

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<var-name>``
     .. rst-class:: text-start

     The name of the cached variable will be
     :variable:`${CETMODULES_CURRENT_PROJECT_NAME}
     <CETMODULES_CURRENT_PROJECT_NAME>`\ ``_<var-name>``

   ``<init-val> ...``
     The initial value(s) of the cached variable.

   Details
   ^^^^^^^

   Order of precedence for the initial value of
   :variable:`!${CETMODULES_CURRENT_PROJECT_NAME}_<var-name>`:

   #. The value of a CMake variable
      :variable:`!${CETMODULES_CURRENT_PROJECT_NAME}_<var-name>` if it
      is defined in the current scope (that in which
      :command:`!project_variable` is called) prior to the definition of
      the project variable.

   #. The value of a CMake variable ``<var-name>`` if it is defined in
      the current scope prior to the definition of the project variable.

   #.
      .. rst-class:: text-start

      The value of a CMake or cached variable
      :variable:`!${CETMODULES_CURRENT_PROJECT_NAME}_<var-name>_INIT`.

   #. ``<init-val> ...``.

   #. ``<backup-var> ...`` if specified and ``<init-val> ...`` evaluates
      to ``FALSE``).


#]================================================================]

function(project_variable VAR_NAME)
  cmake_parse_arguments(
    PARSE_ARGV
    1
    CPV
    "CONFIG;MISSING_OK;NO_WARN_DUPLICATE;NO_WARN_REDUNDANT;OMIT_IF_EMPTY;OMIT_IF_MISSING;OMIT_IF_NULL;PUBLIC"
    "DOCSTRING;TYPE"
    "BACKUP_DEFAULT"
    )
  if(NOT DEFINED
     CACHE{CETMODULES_VARS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}}
     )
    # Cache variable listing all the project variables for the current project.
    set(CETMODULES_VARS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
        CACHE INTERNAL "Valid project variables"
        )
  elseif(VAR_NAME IN_LIST
         CETMODULES_VARS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
         )
    if(NOT CPV_NO_WARN_DUPLICATE)
      message(
        WARNING
          "duplicate definition of project variable ${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME} ignored"
        )
    endif()
    return()
  elseif(VAR_NAME IN_LIST _CPV_RESERVED_NAMES) # Audit requested variable name.
    message(
      SEND_ERROR
        "project variable name ${VAR_NAME} would clash with a variable defined by CMake"
      )
  else()
    message(
      VERBOSE
      "defining project variable ${VAR_NAME} for ${CETMODULES_CURRENT_PROJECT_NAME}"
      )
  endif()
  if(NOT (CPV_CONFIG OR CPV_NO_WARN_REDUNDANT))
    foreach(var IN ITEMS MISSING_OK OMIT_IF_EMPTY OMIT_IF_MISSING OMIT_IF_NULL)
      if(CPV_${var})
        list(APPEND redundant_opts ${var})
      endif()
    endforeach()
    if(redundant_opts)
      message(
        WARNING
          "project_variable(${VAR_NAME}...): these options are redundant if CONFIG not specified: ${redundant_opts}"
        )
    endif()
  endif()
  set_property(
    CACHE CETMODULES_VARS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
    APPEND
    PROPERTY VALUE "${VAR_NAME}"
    )
  if(NOT CPV_DOCSTRING)
    set(CPV_DOCSTRING "Project's setting for ${VAR_NAME}")
  endif()
  if(NOT CPV_TYPE)
    set(CPV_TYPE "PATH_FRAGMENT")
  endif()
  if(CPV_TYPE IN_LIST _CPV_SPECIAL_PATH_TYPES)
    # Need to avoid automatic absolute path conversion for command-line values.
    set(VAR_TYPE "STRING")
  else()
    set(VAR_TYPE "${CPV_TYPE}")
  endif()
  set(ORIGIN)
  # Enforce precedence rules as described above:
  if(DEFINED ${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME}
     AND NOT (DEFINED CACHE{${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME}}
              AND "$CACHE{${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME}}"
                  STREQUAL ${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME})
     )
    # 1.
    set(DEFAULT_VAL "${${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME}}")
    set(FORCE FORCE)
    set(ORIGIN "${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME}")
  elseif(DEFINED ${VAR_NAME})
    # 2.
    set(DEFAULT_VAL "${${VAR_NAME}}")
    set(FORCE FORCE)
    set(ORIGIN "${VAR_NAME}")
  elseif(DEFINED ${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME}_INIT)
    # 4.
    set(DEFAULT_VAL "${${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME}_INIT}")
    set(FORCE FORCE)
    set(ORIGIN "${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME}_INIT")
  else()
    unset(FORCE)
    set(DEFAULT_VAL "${CPV_UNPARSED_ARGUMENTS}")
    # 5.
    set(DEFAULT_ORIGIN "<initial-value>")
    if(NOT DEFAULT_VAL AND CPV_BACKUP_DEFAULT)
      set(DEFAULT_VAL "${CPV_BACKUP_DEFAULT}")
      # 6.
      set(DEFAULT_ORIGIN "<backup-default>")
    endif()
    if(NOT DEFINED CACHE{${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME}})
      set(ORIGIN "${DEFAULT_ORIGIN}")
    endif()
  endif()
  # ############################################################################
  # Avoid hysteresis.
  set(prefix_vars CETMODULES_CURRENT_PROJECT_NAME)
  set(suffixes _INIT)
  foreach(prefix_var suffix IN ZIP_LISTS prefix_vars suffixes)
    foreach(vtype IN ITEMS "" CACHE PARENT_SCOPE)
      unset(${${prefix_var}}_${VAR_NAME}${suffix} ${vtype})
    endforeach()
  endforeach()
  # ############################################################################
  # Make the project variable known to the CMake cache.
  message(
    DEBUG
    "set(${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME} ${DEFAULT_VAL} CACHE ${VAR_TYPE} ${CPV_DOCSTRING} ${FORCE})"
    )
  set(${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME}
      "${DEFAULT_VAL}"
      CACHE ${VAR_TYPE} "${CPV_DOCSTRING}" ${FORCE}
      )
  # Defining cached variable automatically erases the eponymous CMake variable
  # in the current scope only.
  unset(${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME} PARENT_SCOPE)
  if(CPV_PUBLIC)
    set(advanced CLEAR)
  else()
    set(advanced FORCE)
    mark_as_advanced(${advanced} ${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME})
  endif()
  get_property(
    current_val
    CACHE ${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME}
    PROPERTY VALUE
    )
  if(NOT ORIGIN)
    # 4.
    set(ORIGIN "<pre-cached_value>")
  elseif(
    CPV_TYPE IN_LIST _CPV_SPECIAL_PATH_TYPES
    AND ${CETMODULES_CURRENT_PROJECT_NAME}_EXEC_PREFIX
    AND current_val
    AND (VAR_NAME IN_LIST CETMODULES_DEFAULT_ARCH_DIRS
         OR VAR_NAME IN_LIST ADD_ARCH_DIRS
         OR VAR_NAME IN_LIST ${CETMODULES_CURRENT_PROJECT_NAME}_ADD_ARCH_DIRS
         OR VAR_NAME IN_LIST
            ${CETMODULES_CURRENT_PROJECT_NAME}_ADD_ARCH_DIRS_INIT
        )
    AND NOT
        (VAR_NAME IN_LIST ADD_NOARCH_DIRS
         OR VAR_NAME IN_LIST ${CETMODULES_CURRENT_PROJECT_NAME}_ADD_NOARCH_DIRS
         OR VAR_NAME IN_LIST
            ${CETMODULES_CURRENT_PROJECT_NAME}_ADD_NOARCH_DIRS_INIT)
    )
    # Need to prepend EXEC_PREFIX to relative paths.
    set(new_val)
    foreach(path IN LISTS current_val)
      if(NOT IS_ABSOLUTE path)
        string(FIND "${path}"
                    "${${CETMODULES_CURRENT_PROJECT_NAME}_EXEC_PREFIX}" idx
               )
        if(NOT idx EQUAL 0)
          string(PREPEND path
                 "${${CETMODULES_CURRENT_PROJECT_NAME}_EXEC_PREFIX}/"
                 )
        endif()
        list(APPEND new_val "${path}")
      endif()
    endforeach()
    if(NOT
       DEFINED
       CACHE{CETMODULES_${VAR_NAME}_PROPERTIES_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}}
       )
      set(CETMODULES_${VAR_NAME}_PROPERTIES_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
          CACHE
            INTERNAL
            "Properties for project variable ${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME}"
          )
    endif()
    set_property(
      CACHE ${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME} PROPERTY VALUE
                                                                    ${new_val}
      )
  endif()
  # ############################################################################
  # Set "properties" of each project variable that we can interrogate later.
  cet_set_pv_property(${VAR_NAME} PROPERTY ORIGIN "${ORIGIN}")
  cet_set_pv_property(${VAR_NAME} PROPERTY TYPE "${CPV_TYPE}")
  foreach(var IN ITEMS CONFIG OMIT_IF_NULL)
    if(CPV_${var})
      cet_set_pv_property(${VAR_NAME} PROPERTY ${var} TRUE)
    endif()
  endforeach()
  if(CPV_TYPE IN_LIST _CPV_PATH_TYPES)
    cet_set_pv_property(${VAR_NAME} PROPERTY IS_PATH TRUE)
    foreach(var IN ITEMS MISSING_OK OMIT_IF_MISSING)
      if(CPV_${var})
        cet_set_pv_property(${VAR_NAME} PROPERTY ${var} TRUE)
      endif()
    endforeach()
  else()
    foreach(var IN ITEMS MISSING_OK OMIT_IF_MISSING)
      if(CPV_${var})
        message(
          WARNING
            "${var} not valid for project variable ${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME} of TYPE ${CPV_TYPE}"
          )
      endif()
    endforeach()
  endif()
  if(CPV_OMIT_IF_EMPTY)
    if(CPV_TYPE MATCHES [[^PATH(_FRAGMENT)?$]])
      cet_set_pv_property(${VAR_NAME} PROPERTY OMIT_IF_EMPTY TRUE)
    else()
      message(
        WARNING
          "${var} not valid for project variable ${CETMODULES_CURRENT_PROJECT_NAME}_${VAR_NAME} of TYPE ${CPV_TYPE}"
        )
    endif()
  endif()
endfunction()
