cmake_minimum_required(VERSION 3.22)

include_guard()

# User-configurable options
option(CLANG_TIDY_INCLUDE_TESTS "Include test sources/targets in clang-tidy (requires BUILD_TESTING=ON)" ON)
option(CLANG_TIDY_INCLUDE_FORM "Include FORM sources/targets in clang-tidy (when PHLEX_USE_FORM=ON)" ON)

# Find clang-tidy (independent of ENABLE_CLANG_TIDY option)
if(NOT CLANG_TIDY_EXECUTABLE)
  find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy-20 clang-tidy-19 clang-tidy)
endif()

# Prefer run-clang-tidy if available; fall back to clang-tidy
find_program(RUN_CLANG_TIDY_EXECUTABLE NAMES run-clang-tidy-20 run-clang-tidy-19 run-clang-tidy run-clang-tidy.py)

# Determine which .clang-tidy to use (prefer project-level config)
set(_phlex_clang_tidy_config "${PROJECT_SOURCE_DIR}/.clang-tidy")
if(NOT EXISTS "${_phlex_clang_tidy_config}")
  set(_phlex_clang_tidy_config "${CMAKE_SOURCE_DIR}/.clang-tidy")
endif()

# Determine the compilation database (-p) directory.
#
# Prefer CMAKE_BINARY_DIR for multi-project workspaces
# (compile_commands.json is at the top-level), fall back to
# PROJECT_BINARY_DIR if needed.
set(_phlex_compile_db_dir "${CMAKE_BINARY_DIR}")
if(NOT EXISTS "${_phlex_compile_db_dir}/compile_commands.json" AND EXISTS "${PROJECT_BINARY_DIR}/compile_commands.json")
  set(_phlex_compile_db_dir "${PROJECT_BINARY_DIR}")
endif()

# Try to determine clang-tidy version (for diagnostics only)
set(_clang_tidy_version "")
if(CLANG_TIDY_EXECUTABLE)
  execute_process(
    COMMAND "${CLANG_TIDY_EXECUTABLE}" --version
    OUTPUT_VARIABLE _clang_tidy_version_out
    ERROR_VARIABLE _clang_tidy_version_err
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
  )
  string(REGEX MATCH "clang-tidy version ([0-9]+)" _unused "${_clang_tidy_version_out}")
  if(CMAKE_MATCH_1)
    set(_clang_tidy_version "${CMAKE_MATCH_1}")
  endif()
  if(_clang_tidy_version AND _clang_tidy_version VERSION_LESS "19")
    message(WARNING "Detected clang-tidy version ${_clang_tidy_version} (< 19). Proceeding, but version 19+ is recommended.")
  endif()
endif()

# Helper: collect absolute C/C++ sources from all relevant targets,
# honor test/FORM filters, and exclude generated .cxx in
# PROJECT_BINARY_DIR and known rootcling dictionary patterns.
function(phlex_collect_clang_tidy_sources out_var)
  get_property(_all_targets GLOBAL PROPERTY TARGETS)
  set(_accum_sources)

  foreach(_t IN LISTS _all_targets)
    # Skip imported or alias targets
    get_target_property(_is_imported ${_t} IMPORTED)
    if(_is_imported)
      continue()
    endif()

    # Consider only real build targets
    get_target_property(_t_type ${_t} TYPE)
    if(NOT _t_type MATCHES "^(EXECUTABLE|STATIC_LIBRARY|SHARED_LIBRARY|MODULE_LIBRARY|OBJECT_LIBRARY)$")
      continue()
    endif()

    # Target directories
    get_target_property(_t_src_dir ${_t} SOURCE_DIR)
    get_target_property(_t_bin_dir ${_t} BINARY_DIR)

    # Test filtering
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

    # Collect target sources
    get_target_property(_t_sources ${_t} SOURCES)
    if(NOT _t_sources)
      continue()
    endif()

    foreach(_s IN LISTS _t_sources)
      if(NOT _s OR _s MATCHES "^\\$<") # skip generator expressions
        continue()
      endif()

      # Only C/C++ source files
      if(NOT _s MATCHES "\\.(c|cc|cpp|cxx)$")
        continue()
      endif()

      set(_abs "")

      if(IS_ABSOLUTE "${_s}")
        set(_abs "${_s}")
      else()
        # Resolve relative to SOURCE_DIR first
        if(_t_src_dir)
          set(_cand "${_t_src_dir}/${_s}")
          if(EXISTS "${_cand}")
            get_filename_component(_abs "${_cand}" REALPATH)
          endif()
        endif()

        # If not found, resolve relative to BINARY_DIR (generated sources)
        if(NOT _abs AND _t_bin_dir)
          set(_cand2 "${_t_bin_dir}/${_s}")
          if(EXISTS "${_cand2}")
            get_filename_component(_abs "${_cand2}" REALPATH)
          endif()
        endif()

        # Fallback: compute REALPATH with SOURCE_DIR base (even if not present yet)
        if(NOT _abs AND _t_src_dir)
          get_filename_component(_abs "${_s}" REALPATH BASE_DIR "${_t_src_dir}")
        endif()
      endif()

      if(NOT _abs)
        continue()
      endif()

      # Exclude any .cxx under this project's binary dir (covers rootcling and current FORM outputs)
      if(_abs MATCHES "^${PROJECT_BINARY_DIR}/.*\\.(cxx)$")
        continue()
      endif()

      # Extra safety: exclude common rootcling dictionary patterns
      if(_abs MATCHES "([/\\\\]|^)G__.*\\.cxx$"
         OR _abs MATCHES "([/\\\\]|^).*_Dict\\.cxx$"
         OR _abs MATCHES "([/\\\\]|^).*_dict\\.cxx$"
         OR _abs MATCHES "([/\\\\]|^).*_rdict\\.cxx$")
        continue()
      endif()

      list(APPEND _accum_sources "${_abs}")
    endforeach()
  endforeach()

  if(_accum_sources)
    list(REMOVE_DUPLICATES _accum_sources)
  endif()

  set(${out_var} "${_accum_sources}" PARENT_SCOPE)
endfunction()

function(create_clang_tidy_targets)
  if(CLANG_TIDY_EXECUTABLE)
    phlex_collect_clang_tidy_sources(PHLEX_ALL_CXX_SOURCES)

    if(PHLEX_ALL_CXX_SOURCES)
      # If run-clang-tidy is available, use it and filter the file set with a regex
      if(RUN_CLANG_TIDY_EXECUTABLE)
        # Build a regex that matches the discovered file set
        set(_regex_parts)
        foreach(_f IN LISTS PHLEX_ALL_CXX_SOURCES)
          # Escape regex metacharacters
          string(REGEX REPLACE "([][.^$+*?()|\\])" "\\\\\\1" _f_escaped "${_f}")
          list(APPEND _regex_parts "${_f_escaped}")
        endforeach()
        string(JOIN "|" _files_regex ${_regex_parts})
        set(_files_regex "^(${_files_regex})$")

        add_custom_target(
          clang-tidy-check
          COMMAND "${RUN_CLANG_TIDY_EXECUTABLE}"
          -p "${_phlex_compile_db_dir}"
          -clang-tidy-binary "${CLANG_TIDY_EXECUTABLE}"
          -regex "${_files_regex}"
          -- --config-file="${_phlex_clang_tidy_config}"
          COMMENT "Running clang-tidy (via run-clang-tidy) on project sources"
          VERBATIM
        )

        add_custom_target(
          clang-tidy-fix
          COMMAND "${RUN_CLANG_TIDY_EXECUTABLE}"
          -p "${_phlex_compile_db_dir}"
          -clang-tidy-binary "${CLANG_TIDY_EXECUTABLE}"
          -regex "${_files_regex}"
          -fix
          -- --config-file="${_phlex_clang_tidy_config}"
          COMMENT "Applying clang-tidy fixes (via run-clang-tidy) to project sources"
          VERBATIM
        )

        message(STATUS "Clang-tidy targets use run-clang-tidy: ${RUN_CLANG_TIDY_EXECUTABLE} (clang-tidy: ${CLANG_TIDY_EXECUTABLE})")
      else()
        # Fallback: invoke clang-tidy directly on the file list
        add_custom_target(
          clang-tidy-check
          COMMAND "${CLANG_TIDY_EXECUTABLE}"
          -p "${_phlex_compile_db_dir}"
          --config-file="${_phlex_clang_tidy_config}"
          ${PHLEX_ALL_CXX_SOURCES}
          COMMENT "Running clang-tidy checks on project sources"
          VERBATIM
        )

        add_custom_target(
          clang-tidy-fix
          COMMAND "${CLANG_TIDY_EXECUTABLE}"
          -p "${_phlex_compile_db_dir}"
          --config-file="${_phlex_clang_tidy_config}"
          --fix --fix-errors
          ${PHLEX_ALL_CXX_SOURCES}
          COMMENT "Applying clang-tidy fixes to project sources"
          VERBATIM
        )

        message(STATUS "Clang-tidy targets added (direct): ${CLANG_TIDY_EXECUTABLE}")
      endif()
    else()
      add_custom_target(
        clang-tidy-check
        COMMAND ${CMAKE_COMMAND} -E echo "No source files discovered for clang-tidy."
        COMMENT "No source files to run clang-tidy on (filters may have excluded all targets)."
      )
      add_custom_target(
        clang-tidy-fix
        COMMAND ${CMAKE_COMMAND} -E echo "No source files discovered for clang-tidy fixes."
        COMMENT "No source files to run clang-tidy on (filters may have excluded all targets)."
      )
      message(STATUS "Clang-tidy found but no eligible sources; added no-op targets.")
    endif()
  else()
    message(STATUS "clang-tidy not found, skipping clang-tidy targets")
  endif()
endfunction()
