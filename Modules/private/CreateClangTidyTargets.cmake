# CreateClangTidyTargets.cmake
#
# Adds clang-tidy targets for the project with:
#
# * Recursive target discovery via DIRECTORY properties (BUILDSYSTEM_TARGETS,
#   SUBDIRECTORIES)
# * Optional inclusion of tests and FORM targets
# * Robust absolute source path resolution (SOURCE_DIR, BINARY_DIR)
# * Exclusion of generated .cxx under PROJECT_BINARY_DIR and common rootcling
#   patterns
# * Use of run-clang-tidy if available, fallback to clang-tidy
# * Multi-project-safe compile database detection (-p dir)
#
# Options:
#
# CLANG_TIDY_INCLUDE_TESTS (default ON; effective only when BUILD_TESTING=ON)
# CLANG_TIDY_INCLUDE_FORM  (default ON)
#
# Requires: CMake >= 3.22 (project sets this)

# ----------------------------------------------------------------------------
# Preamble
# ----------------------------------------------------------------------------

include_guard()
option(CLANG_TIDY_INCLUDE_TESTS
       "Include test sources/targets in clang-tidy (requires BUILD_TESTING=ON)"
       ON
       )
option(CLANG_TIDY_INCLUDE_FORM
       "Include FORM sources/targets in clang-tidy (when PHLEX_USE_FORM=ON)" ON
       )

# Tool discovery
find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy-20 clang-tidy-19 clang-tidy)
find_program(
  RUN_CLANG_TIDY_EXECUTABLE NAMES run-clang-tidy-20 run-clang-tidy-19
                                  run-clang-tidy run-clang-tidy.py
  )

# clang-tidy version (warn if < 19; do not fail)
set(_clang_tidy_major "")
if(CLANG_TIDY_EXECUTABLE)
  execute_process(
    COMMAND "${CLANG_TIDY_EXECUTABLE}" --version
    OUTPUT_VARIABLE _ct_out
    ERROR_VARIABLE _ct_err
    OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_STRIP_TRAILING_WHITESPACE
    )
  string(REGEX MATCH "clang-tidy version ([0-9]+)" _m "${_ct_out}")
  if(CMAKE_MATCH_1)
    set(_clang_tidy_major "${CMAKE_MATCH_1}")
  endif()
  if(_clang_tidy_major AND _clang_tidy_major VERSION_LESS "19")
    message(
      WARNING
        "Detected clang-tidy version ${_clang_tidy_major} (< 19). Proceeding, but 19+ is recommended."
      )
  endif()
endif()

# Determine which .clang-tidy to use (prefer project-level config)
set(_phlex_clang_tidy_config "${PROJECT_SOURCE_DIR}/.clang-tidy")
if(NOT EXISTS "${_phlex_clang_tidy_config}")
  set(_phlex_clang_tidy_config "${CMAKE_SOURCE_DIR}/.clang-tidy")
endif()

# Determine the compilation database (-p) directory.
#
# Prefer CMAKE_BINARY_DIR for multi-project workspaces (compile_commands.json is
# at the top-level), fall back to PROJECT_BINARY_DIR if needed.
set(_phlex_compile_db_dir "${CMAKE_BINARY_DIR}")
if(NOT EXISTS "${_phlex_compile_db_dir}/compile_commands.json"
   AND EXISTS "${PROJECT_BINARY_DIR}/compile_commands.json"
   )
  set(_phlex_compile_db_dir "${PROJECT_BINARY_DIR}")
endif()

# ----------------------------------------------------------------------------
# Internal helpers
# ----------------------------------------------------------------------------

# Recursively gather all build system targets starting from a source directory
function(_phlex_gather_targets_recursive out_var directory)

  # Targets defined in this directory scope
  get_directory_property(
    _this_dir_targets DIRECTORY "${directory}" BUILDSYSTEM_TARGETS
    )
  if(_this_dir_targets)
    list(APPEND _collected ${_this_dir_targets})
  endif()

  # Recurse into subdirectories created by add_subdirectory()
  get_directory_property(_subs DIRECTORY "${directory}" SUBDIRECTORIES)
  foreach(_sd IN LISTS _subs)
    _phlex_gather_targets_recursive(_child "${_sd}")
    if(_child)
      list(APPEND _collected ${_child})
    endif()
  endforeach()

  if(_collected)
    list(REMOVE_DUPLICATES _collected)
  endif()
  set(${out_var}
      "${_collected}"
      PARENT_SCOPE
      )
endfunction()

# Collect absolute source files for clang-tidy
#
# * honors CLANG_TIDY_INCLUDE_TESTS/FORM
# * resolves sources relative to target SOURCE_DIR and BINARY_DIR
# * excludes generated .cxx under PROJECT_BINARY_DIR and rootcling dictionary
#   patterns
function(_phlex_collect_clang_tidy_sources out_var)
  # Traverse the tree from the current directory
  _phlex_gather_targets_recursive(_all_targets "${CMAKE_CURRENT_BINARY_DIR}")

  # Precompute an escaped PROJECT_BINARY_DIR for regex checks
  set(_proj_bin_dir_re "${PROJECT_BINARY_DIR}")
  # Escape regex metacharacters
  string(REGEX REPLACE "([][.^$+*?()|\\])" "\\\\\\1" _proj_bin_dir_re
                       "${_proj_bin_dir_re}"
         )

  set(_accum)

  foreach(_t IN LISTS _all_targets)
    # Skip imported or alias targets
    get_target_property(_imported ${_t} IMPORTED)
    if(_imported)
      continue()
    endif()

    # Only real build targets
    get_target_property(_type ${_t} TYPE)
    if(NOT
       _type
       MATCHES
       "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$"
       )
      continue()
    endif()

    # Directories for filtering and path resolution
    get_target_property(_t_src_dir ${_t} SOURCE_DIR)
    get_target_property(_t_bin_dir ${_t} BINARY_DIR)

    # Test filtering (only if requested AND BUILD_TESTING=ON)
    if(_t_src_dir AND _t_src_dir MATCHES "([/\\\\]|^)test([/\\\\]|$)")
      if(NOT (CLANG_TIDY_INCLUDE_TESTS AND BUILD_TESTING))
        continue()
      endif()
    endif()

    # FORM filtering
    if(_t_src_dir AND _t_src_dir MATCHES "([/\\\\]|^)form([/\\\\]|$)")
      if(NOT CLANG_TIDY_INCLUDE_FORM)
        continue()
      endif()
    endif()

    # Gather sources
    get_target_property(_sources ${_t} SOURCES)
    if(NOT _sources)
      continue()
    endif()

    foreach(_s IN LISTS _sources)
      # Skip generator expressions and empty
      if(NOT _s OR _s MATCHES "^\\$<")
        continue()
      endif()

      # Only C-family source files
      if(NOT _s MATCHES "\\.(c|cc|cpp|cxx)$")
        continue()
      endif()

      set(_abs "")
      if(IS_ABSOLUTE "${_s}")
        set(_abs "${_s}")
      else()
        # Try target SOURCE_DIR
        if(_t_src_dir)
          set(_cand "${_t_src_dir}/${_s}")
          if(EXISTS "${_cand}")
            get_filename_component(_abs "${_cand}" ABSOLUTE)
          endif()
        endif()
        # Try target BINARY_DIR (generated sources)
        if(NOT _abs AND _t_bin_dir)
          set(_cand2 "${_t_bin_dir}/${_s}")
          if(EXISTS "${_cand2}")
            get_filename_component(_abs "${_cand2}" ABSOLUTE)
          endif()
        endif()
        # Fallback: ABSOLUTE relative to SOURCE_DIR even if not yet generated
        if(NOT _abs AND _t_src_dir)
          get_filename_component(_abs "${_s}" ABSOLUTE BASE_DIR "${_t_src_dir}")
        endif()
      endif()

      if(NOT _abs)
        continue()
      endif()

      # Exclude any generated .cxx under this project's binary tree
      if(_abs MATCHES "^${_proj_bin_dir_re}(/|\\\\).+\\.cxx$")
        continue()
      endif()

      # Extra: common rootcling dictionary patterns
      if(_abs MATCHES "([/\\\\]|^)G__.*\\.cxx$"
         OR _abs MATCHES "([/\\\\]|^).*_Dict\\.cxx$"
         OR _abs MATCHES "([/\\\\]|^).*_dict\\.cxx$"
         OR _abs MATCHES "([/\\\\]|^).*_rdict\\.cxx$"
         )
        continue()
      endif()

      list(APPEND _accum "${_abs}")
    endforeach()
  endforeach()

  if(_accum)
    list(REMOVE_DUPLICATES _accum)
  endif()
  set(${out_var}
      "${_accum}"
      PARENT_SCOPE
      )
endfunction()

# ----------------------------------------------------------------------------
# Public entry point: preserve function name/signature
# ----------------------------------------------------------------------------
function(create_clang_tidy_targets)
  cmake_language(
    DEFER DIRECTORY "${PROJECT_SOURCE_DIR}" CALL
    _create_clang_tidy_targets_impl
    )
endfunction()

function(_create_clang_tidy_targets_impl)
  # Check for CLANG_TIDY_EXECUTABLE
  if(NOT CLANG_TIDY_EXECUTABLE)
    message(STATUS "clang-tidy not found, skipping clang-tidy targets")
    return()
  endif()

  _phlex_collect_clang_tidy_sources(PHLEX_ALL_CXX_SOURCES)

  if(PHLEX_ALL_CXX_SOURCES)
    if(RUN_CLANG_TIDY_EXECUTABLE)
      # Build a regex matching the exact discovered file set
      set(_regex_parts)
      foreach(_f IN LISTS PHLEX_ALL_CXX_SOURCES)
        # Escape regex metacharacters
        string(REGEX REPLACE "([][.^$+*?()|\\])" "\\\\\\1" _f_escaped "${_f}")
        list(APPEND _regex_parts "${_f_escaped}")
      endforeach()
      string(JOIN "|" _files_regex ${_regex_parts})
      set(_files_regex "^(${_files_regex})")

      # Create a small wrapper script in the build tree that reads runtime
      # environment variables to allow selective fixing (PHLEX_TIDY_CHECKS,
      # PHLEX_TIDY_FILES). The script is generated from RunClangTidyFix.sh.in
      # and marked executable.
      set(_phlex_run_clang_tidy_sh
          "${CMAKE_BINARY_DIR}/phlex_run_clang_tidy_fix.sh"
          )
      configure_file(
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/RunClangTidyFix.sh.in"
        "${_phlex_run_clang_tidy_sh}"
        @ONLY
        FILE_PERMISSIONS
        OWNER_READ
        OWNER_WRITE
        OWNER_EXECUTE
        GROUP_READ
        GROUP_EXECUTE
        WORLD_READ
        WORLD_EXECUTE
        )

      add_custom_target(
        clang-tidy-check
        COMMAND "${_phlex_run_clang_tidy_sh}"
        COMMENT "Running clang-tidy (via run-clang-tidy) on project sources"
        VERBATIM
        )

      add_custom_target(
        clang-tidy-fix
        COMMAND "${_phlex_run_clang_tidy_sh}" --fix
        COMMENT
          "Applying clang-tidy fixes (via run-clang-tidy) to project sources"
        VERBATIM
        )

      message(
        STATUS
          "Clang-tidy targets use run-clang-tidy (wrapper): ${RUN_CLANG_TIDY_EXECUTABLE} (clang-tidy: ${CLANG_TIDY_EXECUTABLE})"
        )
    else()
      add_custom_target(
        clang-tidy-check
        COMMAND
          "${CLANG_TIDY_EXECUTABLE}" -p "${_phlex_compile_db_dir}"
          --config-file="${_phlex_clang_tidy_config}" ${PHLEX_ALL_CXX_SOURCES}
        COMMENT "Running clang-tidy checks on project sources"
        VERBATIM
        )

      add_custom_target(
        clang-tidy-fix
        COMMAND
          "${CLANG_TIDY_EXECUTABLE}" -p "${_phlex_compile_db_dir}"
          --config-file="${_phlex_clang_tidy_config}" --fix --fix-errors
          ${PHLEX_ALL_CXX_SOURCES}
        COMMENT "Applying clang-tidy fixes to project sources"
        VERBATIM
        )

      message(
        STATUS "Clang-tidy targets added (direct): ${CLANG_TIDY_EXECUTABLE}"
        )
    endif()
  else()
    add_custom_target(
      clang-tidy-check
      COMMAND ${CMAKE_COMMAND} -E echo
              "No source files discovered for clang-tidy."
      COMMENT
        "No source files to run clang-tidy on (filters may have excluded all targets)."
      )
    add_custom_target(
      clang-tidy-fix
      COMMAND ${CMAKE_COMMAND} -E echo
              "No source files discovered for clang-tidy fixes."
      COMMENT
        "No source files to run clang-tidy on (filters may have excluded all targets)."
      )
    message(
      STATUS "Clang-tidy found but no eligible sources; added no-op targets."
      )
  endif()
endfunction()
