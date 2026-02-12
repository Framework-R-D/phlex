#[================================================================[.rst:
CetTransitivePaths
------------------

Defines the function :commmand:`cet_transitive_paths`.

#]================================================================]

include_guard()

include(CetCMakeUtils)

#[================================================================[.rst:
.. command:: cet_transitive_paths


   Get a list of path values for all transitive dependencies of the
   given project name.  For a given path name, the dependency tree of
   the given project is searched according to the dependencies
   specified via ``find_package``.

   For example, if the path name ``"MY_PATH"`` is supplied, the
   function will traverse the dependency structure and collect all
   values of the path name ``"<project-or-dependency name>_MY_PATH"``.
   The result can be accessed via the variable
   ``"TRANSITIVE_PATHS_WITH_MY_PATH"``.

   .. code-block:: cmake

      cet_transitive_paths(<path-name> [<project-variable localization options>] [IN_TREE] [PROJECT <project-name>])

   .. seealso:: :command:`cet_localize_pv`

   Options
   ^^^^^^^

   ``[PROJECT] <project-name>``
     The top-level project from which the transitive dependency
     traversal occurs.  (default
     :variable:`${CETMODULES_CURRENT_PROJECT_NAME}
     <CETMODULES_CURRENT_PROJECT_NAME>`).

   ``[IN_TREE]``
     Include paths only from projects that are included in the build tree.

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<path-name>``
     The name of the path that serves as the suffix for the top-level
     project and all transitive dependencies.  The path name can be a
     Cetmodules project variable, in which case the options to
     :command:`cet_localize_pv` may also be provided.

#]================================================================]

function(cet_transitive_paths PATH)
  cmake_parse_arguments(PARSE_ARGV 1 CTP "IN_TREE" "PROJECT" "")

  set(PKG ${CETMODULES_CURRENT_PROJECT_NAME})
  if(CTP_PROJECT)
    set(PKG ${CTP_PROJECT})
  endif()

  _cet_transitive_project_names(${PKG} PNAMES_RESULT)
  foreach(DEP IN LISTS PNAMES_RESULT)
    if(DEFINED CACHE{CETMODULES_${PATH}_PROPERTIES_PROJECT_${DEP}})
      cet_localize_pv(${DEP} ${PATH} ${CTP_UNPARSED_ARGUMENTS})
    endif()
    list(APPEND TRANSITIVE_PATHS_TMP ${${DEP}_${PATH}})
  endforeach()
  set(TRANSITIVE_PATHS_WITH_${PATH}
      ${TRANSITIVE_PATHS_TMP}
      PARENT_SCOPE
      )
endfunction()

function(_cet_transitive_project_names PKG RESULT_LIST)
  set(RESULT_LIST_TMP ${${RESULT_LIST}})
  foreach(DEP IN LISTS CETMODULES_FIND_DEPS_PNAMES_PROJECT_${PKG})
    if(PKG STREQUAL DEP OR (CTP_IN_TREE AND NOT ${DEP}_IN_TREE))
      continue()
    endif()
    if(NOT "${DEP}" IN_LIST RESULT_LIST_TMP)
      _cet_transitive_project_names("${DEP}" RESULT_LIST_TMP)
    endif()
  endforeach()
  list(PREPEND RESULT_LIST_TMP ${PKG})
  set(${RESULT_LIST}
      ${RESULT_LIST_TMP}
      PARENT_SCOPE
      )
endfunction()
