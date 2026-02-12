#[================================================================[.rst:
CetGenerateSphinxDocs
---------------------
#]================================================================]
include_guard()

cmake_minimum_required(VERSION 3.20...4.1 FATAL_ERROR)

set(_CGSD_VDATA_VERSION 1)

include(CetCMakeUtils)
include(CetPackagePath)
include(ParseVersionString)
include(ProjectVariable)

function(cet_generate_sphinx_docs)
  if(NOT BUILD_DOCS) # Disabled.
    return()
  endif()

  # In-package installation location (default ${CMAKE_INSTALL_DOCDIR}).
  project_variable(
    SPHINX_DOC_DIR
    "${${CETMODULES_CURRENT_PROJECT_NAME}_DOC_DIR}"
    NO_WARN_DUPLICATE
    BACKUP_DEFAULT
    ${CMAKE_INSTALL_DOCDIR}
    DOCSTRING
    "Location of installed Sphinx-generated documentation for ${CETMODULES_CURRENT_PROJECT_NAME}"
    )

  # Require sphinx-doc.
  find_package(sphinx-doc 3.0 QUIET REQUIRED)

  # Parse arguments.
  set(flags
      NITPICKY
      NOP
      NO_ALL
      NO_COLOR
      NO_CONF
      NO_DELETE_OUTPUT_DIR
      NO_INSTALL
      QUIET
      VERBOSE
      )
  set(one_arg_opts
      CONF_DIR
      SOURCE_DIR
      SWITCH_VERSION
      TARGET_STEM
      TARGETS_VAR
      VERBOSITY
      VERSION_DATA_VAR
      )
  set(options DEPENDS EXTRA_ARGS OUTPUT_FORMATS)
  set(pf_keywords
      ALL
      COLOR
      DELETE_OUTPUT_DIR
      DEPENDS
      EXTRA_ARGS
      INSTALL
      NITPICKY
      OUTPUT_DIR
      QUIET
      VERBOSE
      VERBOSITY
      )
  string(REPLACE ";" "|" pf_kw_re "(${pf_keywords})")
  list(TRANSFORM ARGV REPLACE "(^|_)NO_" "\\1" OUTPUT_VARIABLE fmt_args)
  list(FILTER fmt_args INCLUDE REGEX "^(.+)_${pf_kw_re}$")
  list(TRANSFORM fmt_args REPLACE "^([^_]+)_${pf_kw_re}$" "\\1")
  list(REMOVE_DUPLICATES fmt_args)
  foreach(fmt IN LISTS fmt_args)
    list(
      APPEND
      flags
      ${fmt}_ALL
      ${fmt}_COLOR
      ${fmt}_DELETE_OUTPUT_DIR
      ${fmt}_INSTALL
      ${fmt}_NITPICKY
      ${fmt}_NO_ALL
      ${fmt}_NO_COLOR
      ${fmt}_NO_DELETE_OUTPUT_DIR
      ${fmt}_NO_INSTALL
      ${fmt}_NO_NITPICKY
      ${fmt}_NO_QUIET
      ${fmt}_NO_VERBOSE
      ${fmt}_QUIET
      ${fmt}_VERBOSE
      )
    list(APPEND one_arg_opts ${fmt}_OUTPUT_DIR ${fmt}_VERBOSITY)
    list(APPEND options ${fmt}_DEPENDS ${fmt}_EXTRA_ARGS)
  endforeach()
  cmake_parse_arguments(
    PARSE_ARGV 0 CGS "${flags}" "${one_arg_opts}" "${options}"
    )

  # Post-process arguments.
  _cgs_process_args()

  # Generate targets
  _cgs_generate_targets()
endfunction()

function(cet_publish_sphinx_html PUBLISH_ROOT PUBLISH_VERSION)
  if(NOT IS_ABSOLUTE "${PUBLISH_ROOT}")
    message(FATAL_ERROR "PUBLISH_ROOT must be absolute (${PUBLISH_ROOT})")
  endif()
  if("PUBLISH_OLD_RELEASE" IN_LIST ARGN)
    set(CPS_PUBLISH_OLD_RELEASE TRUE)
    list(TRANSFORM ARGN REPLACE "^PUBLISH_OLD_RELEASE$" "NOP")
  endif()
  list(FIND ARGN "VERSION_DATA_VAR" idx)
  if(idx GREATER -1)
    list(REMOVE_AT ARGN ${idx})
    list(GET ARGN ${idx} CPS_VERSION_DATA_VAR)
    list(REMOVE_AT ARGN ${idx})
    if(idx GREATER 0)
      list(INSERT ARGN ${idx} "NOP")
    endif()
  endif()
  list(FIND ARGN "TARGETS_VAR" idx)
  if(idx GREATER -1)
    math(EXPR idx "${idx} + 1")
    list(GET ARGN ${idx} CPS_TARGETS_VAR)
  endif()
  set(PUBLISH_ARGS EXTRA_ARGS -A versionswitch=1)

  if(PUBLISH_VERSION MATCHES "^[0-9]+(\\.[0-9]+)?")
    set(_cps_is_numeric TRUE)
  endif()

  # Determine output location.
  if(_cps_is_numeric)
    set(_cps_output_dir "${PUBLISH_ROOT}/v${PUBLISH_VERSION}")
    if(_cps_is_numeric
       AND NOT CPS_PUBLISH_OLD_RELEASE
       AND EXISTS "${_cps_output_dir}_static/documentation_options.js"
       )
      # We've already published documentation for this version: is ours for an
      # earlier release?
      file(READ "${_cps_output_dir}_static/documentation_options.js" doc_data
           )# Read the info for the generated documentation.
      # Extract the documentation release:
      if(doc_data MATCHES "\n[ \t]*VERSION[ \t\n]*:[ \t\n]*'([^']*)'")
        set(found_release "${CMAKE_MATCH_1}")
        cet_compare_versions(
          found_newer "${found_release}" VERSION_GREATER
          "${${CETMODULES_CURRENT_PROJECT_NAME}_CURRENT_PROJECT_VERSION}"
          )
        if(found_newer)
          # Skip publication of documentation for an older release.
          message(
            NOTICE
            "\
found already-published ${CETMODULES_CURRENT_PROJECT_NAME} \
documentation for newer release of version ${PUBLISH_VERSION} \
(${found_release} > ${${CETMODULES_CURRENT_PROJECT_NAME}_CURRENT_PROJECT_VERSION}): will not overwrite (set \
use PUBLISH_OLD_RELEASE flag to force)\
"
            )
          return()
        endif()
      endif()
    endif()
  else()
    set(_cps_output_dir "${PUBLISH_ROOT}/${PUBLISH_VERSION}")
  endif()
  cmake_path(GET _cps_output_dir FILENAME _cps_proj_dir)

  _cps_process_version_data()

  set(target_stem publish_${CETMODULES_CURRENT_PROJECT_NAME}_${PUBLISH_VERSION})
  list(
    APPEND
    PUBLISH_ARGS
    NOP
    NO_INSTALL
    html_OUTPUT_DIR
    "${_cps_output_dir}"
    TARGET_STEM
    ${target_stem}
    )

  cet_generate_sphinx_docs(${ARGN} ${PUBLISH_ARGS})

  set(publish_target sphinx-doc-${target_stem}_html)

  add_custom_target(
    pre_clean_${publish_target}${tlabel}
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${_cps_output_dir}"
    COMMENT
      "\
removing existing published documentation for ${CETMODULES_CURRENT_PROJECT_NAME} ${PUBLISH_VERSION}\
"
    )
  add_dependencies(${publish_target} pre_clean_${publish_target})
  add_dependencies(${publish_target}-force pre_clean_${publish_target})
  set_property(
    DIRECTORY
    APPEND
    PROPERTY ADDITIONAL_CLEAN_FILES "${_cps_output_dir}"
    )

  if(_cps_is_latest)
    # Add the (re-)generation of the "latest" link to the generation target.
    add_custom_command(
      TARGET ${publish_target}
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E rm -f "${PUBLISH_ROOT}/latest"
      COMMAND ${CMAKE_COMMAND} -E create_symlink "${_cps_proj_dir}"
              "${PUBLISH_ROOT}/latest"
      COMMENT "${publish_target}: latest -> ${_cps_proj_dir}"
      )
  endif()

  if(CPS_VERSION_DATA_VAR)
    set(${CPS_VERSION_DATA_VAR}
        "${VERSION_DATA}"
        PARENT_SCOPE
        )
  endif()

  if(CPS_TARGETS_VAR)
    set(${CPS_TARGETS_VAR}
        "${${CPS_TARGETS_VAR}}"
        PARENT_SCOPE
        )
  endif()
endfunction()

macro(_cgs_generate_targets)
  set(targets)
  set(dirs)
  foreach(fmt IN LISTS CGS_OUTPUT_FORMATS)
    set(sphinx_build_args)
    set(target_stem "${CGS_TARGET_STEM}_${fmt}")
    if(CGS_${fmt}_OUTPUT_DIR)
      set(output_dir "${CGS_${fmt}_OUTPUT_DIR}")
    else()
      set(output_dir ${fmt})
    endif()
    if(CGS_${fmt}_NO_ALL OR (CGS_NO_ALL AND NOT CGS_${fmt}_ALL))
      set(all)
    else()
      set(all ALL)
    endif()
    if(CGS_${fmt}_NO_INSTALL OR (CGS_NO_INSTALL AND NOT CGS_${fmt}_INSTALL))
      set(no_install TRUE)
    else()
      set(no_install)
    endif()
    if(CGS_${fmt}_NO_COLOR OR (CGS_NO_COLOR AND NOT CFS_${fmt}_COLOR))
      list(APPEND sphinx_build_args -N)
    else()
      list(APPEND sphinx_build_args --color)
    endif()
    if(CGS_${fmt}_NITPICKY OR (CGS_NITPICKY AND NOT CGS_${fmt}_NO_NITPICKY))
      list(APPEND sphinx_build_args -n)
    endif()
    if(CGS_${fmt}_QUIET OR (CGS_QUIET AND NOT CGS_${fmt}_NO_QUIET))
      list(APPEND sphinx_build_args -q)
    elseif(CGS_${fmt}_NO_VERBOSE OR (DEFINED CFS_${fmt}_VERBOSITY
                                     AND NOT CFS_${fmt}_VERBOSITY)
           )
      list(FILTER sphinx_build_args EXCLUDE REGEX "^-v+$")
    else()
      list(REMOVE_ITEM sphinx_build_args -q)
      if(DEFINED CGS_VERBOSITY AND NOT DEFINED CGS_${fmt}_VERBOSITY)
        set(CGS_${fmt}_VERBOSITY ${CGS_VERBOSITY})
      endif()
      if(CGS_${fmt}_VERBOSITY MATCHES "^-v+$")
        list(APPEND sphinx_build_args ${CGS_${fmt}_VERBOSITY})
      elseif(CGS_${fmt}_VERBOSITY MATCHES "^[0-9]+$")
        string(REPEAT "v" ${CGS_${fmt}_VERBOSITY} tmp)
        if(tmp)
          list(APPEND sphinx_build_args "-${tmp}")
        endif()
      elseif(NOT sphinx_build_args MATCHES "(^|;)-v+(;|$)"
             AND (CGS_VERBOSE OR CGS_${fmt}_VERBOSE)
             )
        list(APPEND sphinx_build_args "-v")
      endif()
    endif()
    if(CGS_EXTRA_ARGS MATCHES "(^|;)(-q|-v+)(;|$)")
      list(REMOVE_ITEM sphinx_build_args -q)
      list(FILTER sphinx_build_args EXCLUDE REGEX "^-v+$")
    endif()
    if(CGS_${fmt}_EXTRA_ARGS MATCHES "(^|;)(-q|-v+)(;|$)")
      list(REMOVE_ITEM sphinx_build_args -q)
      list(REMOVE_ITEM CGS_EXTRA_ARGS -q)
      list(FILTER sphinx_build_args EXCLUDE REGEX "^-v+$")
      list(FILTER CGS_EXTRA_ARGS EXCLUDE REGEX "^-v+$")
    endif()
    list(APPEND sphinx_build_args ${CGS_EXTRA_ARGS} ${CGS_${fmt}_EXTRA_ARGS})
    set(target sphinx-doc-${target_stem})
    if("${sphinx_build_args}" MATCHES "(^|;)-w;([^;]+)(;|$)")
      set(warnings_log "${CMAKE_MATCH_1}")
    else()
      set(warnings_log "${target}-warnings.log")
      list(APPEND CGS_${fmt}_EXTRA_ARGS -w "${warnings_log}")
    endif()
    set(cmd_args -b ${fmt} ${sphinx_build_args} "${CGS_SOURCE_DIR}"
                 ${output_dir}
        )
    # Failure semantics aren't great for sphinx-build: need to wrap. We must
    # delete the whole ${fmt}/ directory on failure otherwise we have an
    # hysteresis problem.
    set(extra_args-force -E)
    set(extra_args)
    set(all-force)
    set(extra_defines)
    if(CGS_${fmt}_DELETE_OUTPUT_DIR OR NOT (CGS_NO_DELETE_OUTPUT_DIR
                                            OR CGS_${fmt}_NO_DELETE_OUTPUT_DIR)
       )
      list(APPEND extra_defines "-DCMD_DELETE_ON_FAILURE=${output_dir}")
      set_property(
        DIRECTORY
        APPEND
        PROPERTY ADDITIONAL_CLEAN_FILES "${output_dir}"
        )
    endif()
    set(cache_dir "${CMAKE_CURRENT_BINARY_DIR}/.doctrees-${target}")
    if(CGS_NO_CONF)
      set(depends_conf)
    else()
      set(depends_conf "${CGS_CONF_DIR}/conf.py")
    endif()
    foreach(tlabel "" -force)
      set(cmd_args${tlabel} ${extra_args${tlabel}} ${cmd_args})
      add_custom_target(
        ${target}${tlabel}
        ${all${tlabel}}
        COMMAND
          ${CMAKE_COMMAND} -DCMD=$<TARGET_FILE:sphinx-doc::sphinx-build>
          -DCMD_ARGS="${cmd_args${tlabel}};-d;${cache_dir}" ${extra_defines} -P
          "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/CetCmdWrapper.cmake"
        DEPENDS ${CGS_DEPENDS} ${CGS_${fmt}_DEPENDS} ${depends_conf}
        COMMAND_EXPAND_LISTS
        COMMENT "Building ${fmt} documentation for ${target} with sphinx-build"
        JOB_POOL sphinx_doc
        )
      if(fmt STREQUAL man)
        cet_localize_pv(cetmodules LIBEXEC_DIR)
        add_custom_command(
          TARGET ${target}${tlabel}
          POST_BUILD
          COMMAND ${cetmodules_LIBEXEC_DIR}/fix-man-dirs ${fmt}
          COMMENT "Renaming manual section directories for ${target}"
          VERBATIM
          )
      endif()
    endforeach()
    if(warnings_log)
      set_property(
        DIRECTORY
        APPEND
        PROPERTY ADDITIONAL_CLEAN_FILES "${warnings_log}"
        )
    endif()
    set_property(
      DIRECTORY
      APPEND
      PROPERTY ADDITIONAL_CLEAN_FILES "${cache_dir}"
      )
    list(APPEND targets ${target})
    if(NOT TARGET sphinx-doc-force)
      add_custom_target(
        sphinx-doc-force
        COMMENT "Building documentation with Sphinx"
        JOB_POOL sphinx_doc
        )
    endif()
    add_dependencies(sphinx-doc-force ${target}-force)
    if(all)
      if(NOT TARGET sphinx-doc)
        add_custom_target(
          sphinx-doc ALL
          COMMENT "Building documentation with Sphinx"
          JOB_POOL sphinx_doc
          )
      endif()
      add_dependencies(sphinx-doc ${target})
      if(NOT TARGET doc)
        add_custom_target(doc ALL COMMENT "Building documentation")
        add_dependencies(doc sphinx-doc)
      endif()
      set(efa_arg)
    else()
      set(efa_arg EXCLUDE_FROM_ALL)
    endif()
    if(NOT no_install)
      if(fmt STREQUAL "man")
        set(fmt "${fmt}/")
        set(install_dir_pv MAN_DIR)
      else()
        set(install_dir_pv SPHINX_DOC_DIR)
      endif()
      install(
        DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${fmt}" ${efa_arg}
        DESTINATION "${${CETMODULES_CURRENT_PROJECT_NAME}_${install_dir_pv}}"
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ
        DIRECTORY_PERMISSIONS
          OWNER_READ
          OWNER_WRITE
          OWNER_EXECUTE
          GROUP_READ
          GROUP_EXECUTE
          WORLD_READ
          WORLD_EXECUTE
        )
    endif()
  endforeach()
  if(CGS_TARGETS_VAR)
    set(${CGS_TARGETS_VAR}
        "${targets}"
        PARENT_SCOPE
        )
  endif()
endmacro()

macro(_cgs_process_args)
  if(NOT CGS_SOURCE_DIR)
    set(CGS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()
  if(CGS_OUTPUT_FORMATS)
    set(missing_fmts)
    foreach(fmt IN LISTS fmt_args)
      if(NOT fmt IN_LIST CGS_OUTPUT_FORMATS)
        list(APPEND missing_fmts "${fmt}")
      endif()
    endforeach()
    if(missing_fmts)
      message(
        WARNING
          "options specified for non-requested document formats for ${CETMODULES_CURRENT_PROJECT_NAME}:
  ${missing_fmts}\
"
        )
    endif()
  else()
    set(CGS_OUTPUT_FORMATS ${fmt_args})
  endif()
  if(NOT CGS_OUTPUT_FORMATS)
    if("${CGS_SWITCH_VERSION}" STREQUAL "")
      return()
    else()
      set(CGS_OUTPUT_FORMATS html)
    endif()
  elseif(NOT "${CGS_SWITCH_VERSION}" STREQUAL "")
    if(NOT CGS_OUTPUT_FORMATS STREQUAL "html")
      message(
        FATAL_ERROR
          "version switching only valid for html output \
(selected ${CGS_OUTPUT_FORMATS} for version ${CGS_SWITCH_VERSION})\
"
        )
    endif()
  elseif("man" IN_LIST CGS_OUTPUT_FORMATS)
    project_variable(
      MAN_DIR ${CMAKE_INSTALL_MANDIR} NO_WARN_DUPLICATE DOCSTRING
      "Location of installed U**X [GT]ROFF-format manuals for \
${CETMODULES_CURRENT_PROJECT_NAME}"
      )
  endif()
  if(CGS_NO_CONF)
    if(CGS_CONF_DIR)
      message(FATAL_ERROR "CONF_DIR and NO_CONF are mutually exclusive")
    endif()
    list(APPEND CGS_EXTRA_ARGS -C)
    set(conf_dep)
  else()
    if(NOT CGS_CONF_DIR)
      set(CGS_CONF_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()
    list(APPEND CGS_EXTRA_ARGS -c "${CGS_CONF_DIR}")
  endif()
  if(NOT CGS_TARGET_STEM)
    cet_package_path(current_directory SOURCE)
    string(REPLACE "/" "_" CGS_TARGET_STEM "${current_directory}")
    string(PREPEND CGS_TARGET_STEM "${CETMODULES_CURRENT_PROJECT_NAME}_")
  endif()
endmacro()

macro(_cps_process_version_data)
  unset(VERSION_DATA)
  if(EXISTS "${PUBLISH_ROOT}/versions.json")
    file(READ "${PUBLISH_ROOT}/versions.json" VERSION_DATA)
  endif()
  string(
    JSON
    _cps_vdata_version
    ERROR_VARIABLE
    json_error
    GET
    "${VERSION_DATA}"
    "vdata-version"
    )
  if(json_error)
    set(_cps_vdata_version 0)
  endif()
  foreach(v RANGE ${_cps_vdata_version} ${_CGSD_VDATA_VERSION})
    cmake_language(CALL _cps_read_version_data_${v})
  endforeach()
  # How many versions are currently defined?
  string(JSON n_versions LENGTH "${VERSION_DATA}" "version-entries")
  set(_cps_is_latest ${_cps_is_numeric})
  # Analyze them.
  set(numeric_versions)
  set(named_versions)
  if(n_versions GREATER 0)
    math(EXPR last_idx "${n_versions} - 1")
    foreach(idx RANGE ${last_idx})
      string(JSON version MEMBER "${VERSION_DATA}" "version-entries" ${idx})
      if(version STREQUAL "latest")
        set(have_latest TRUE)
      elseif(PUBLISH_VERSION STREQUAL version)
        set(have_version TRUE)
      endif()
      if(version MATCHES "^[0-9]+(.[0-9]+)?")
        if(_cps_is_latest)
          # Are we still the latest version?
          cet_compare_versions(
            _cps_is_latest ${PUBLISH_VERSION} VERSION_GREATER_EQUAL ${version}
            )
        endif()
        list(APPEND numeric_versions ${version})
      else()
        list(APPEND named_versions ${version})
      endif()
    endforeach()
  endif()
  # Ensure we have a line for this version in versions data.
  if(NOT have_version)
    string(
      JSON
      VERSION_DATA
      SET
      "${VERSION_DATA}"
      "version-entries"
      "${PUBLISH_VERSION}"
      "{ \"display-name\": \"${_cps_proj_dir}\", \"description\": \"${PUBLISH_VERSION}\" }"
      )
    if(_cps_is_numeric)
      list(APPEND numeric_versions ${PUBLISH_VERSION})
    else()
      list(APPEND named_versions ${PUBLISH_VERSION})
    endif()
  endif()
  if(NOT have_latest)
    # Ensure we a line for "latest" in versions data.
    string(
      JSON
      VERSION_DATA
      SET
      "${VERSION_DATA}"
      "version-entries"
      "latest"
      "{ \"display-name\": \"latest release\", \"description\": \"latest\" }"
      )
    list(APPEND named_versions latest)
  endif()
  # Write out the desired order of versions.
  list(
    SORT named_versions
    COMPARE STRING
    CASE INSENSITIVE
    )
  list(
    SORT numeric_versions
    COMPARE NATURAL
    CASE INSENSITIVE
    ORDER DESCENDING
    )
  if(numeric_versions)
    list(GET numeric_versions 0 latest_numeric)
    string(JSON VERSION_DATA SET "${VERSION_DATA}" "latest-version"
           "\"${latest_numeric}\""
           )
  elseif(NOT have_latest)
    set(_cps_is_latest TRUE)
    string(JSON VERSION_DATA SET "${VERSION_DATA}" "latest-version"
           "\"${PUBLISH_VERSION}\""
           )
  endif()
  string(JSON VERSION_DATA SET "${VERSION_DATA}" "ordered-versions" "[]")
  set(idx 0)
  foreach(version IN LISTS named_versions numeric_versions)
    string(
      JSON
      VERSION_DATA
      SET
      "${VERSION_DATA}"
      "ordered-versions"
      ${idx}
      "\"${version}\""
      )
    math(EXPR idx "${idx} + 1")
  endforeach()
  # Write out the versions data as (possibly) amended by us.
  file(WRITE "${PUBLISH_ROOT}/versions.json" "${VERSION_DATA}\n")
endmacro()

function(_cps_read_version_data_1)

endfunction()

function(_cps_read_version_data_0)
  set(vdata_new "{}")
  string(JSON vdata_new SET "${vdata_new}" "vdata-version" 1)
  string(JSON vdata_new SET "${vdata_new}" "version-entries" "{}")
  string(JSON n_versions ERROR_VARIABLE json_error LENGTH "${VERSION_DATA}")
  if(n_versions)
    math(EXPR last_idx "${n_versions} - 1")
    foreach(idx RANGE ${last_idx})
      string(JSON version MEMBER "${VERSION_DATA}" ${idx})
      set(display_name "${version}")
      string(JSON description GET "${VERSION_DATA}" "${display_name}")
      if(version MATCHES "^v([0-9]+(.[0-9]+)?)")
        string(JSON version GET "${VERSION_DATA}" "${display_name}")
      endif()
      set(vdata
          "{ \"display-name\": \"${display_name}\", \"description\": \"${description}\" }"
          )
      string(
        JSON
        vdata_new
        SET
        "${vdata_new}"
        "version-entries"
        "${version}"
        "${vdata}"
        )
    endforeach()
  endif()
  message(STATUS "VDATA_VERSION 0: ${VERSION_DATA}")
  message(STATUS "VDATA_VERSION 1: ${vdata_new}")
  set(VERSION_DATA
      "${vdata_new}"
      PARENT_SCOPE
      )
endfunction()

include_guard(GLOBAL)

get_property(_cgs_job_pools GLOBAL PROPERTY JOB_POOLS)
if(NOT _cgs_job_pools MATCHES "(^|;)sphinx_doc=[0-9]")
  set_property(GLOBAL APPEND PROPERTY JOB_POOLS sphinx_doc=1)
endif()
