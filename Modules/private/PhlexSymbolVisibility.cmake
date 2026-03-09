include(GenerateExportHeader)

function(phlex_apply_symbol_visibility target)
  set(EXPORT_HEADER "${PROJECT_BINARY_DIR}/include/phlex/${target}_export.hpp")
  set(EXPORT_MACRO_NAME "${target}_EXPORT")

  generate_export_header(
    ${target}
    BASE_NAME ${target}
    EXPORT_FILE_NAME ${EXPORT_HEADER}
    EXPORT_MACRO_NAME ${EXPORT_MACRO_NAME}
    STATIC_DEFINE "${target}_STATIC_DEFINE"
  )

  set_target_properties(
    ${target}
    PROPERTIES CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN ON
  )

  target_include_directories(
    ${target} PUBLIC $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>
  )

  install(FILES "${EXPORT_HEADER}" DESTINATION include/phlex)
endfunction()

# Create a non-installed companion library <target>_internal with default (visible) symbol
# visibility for all symbols. This allows tests to access non-exported implementation
# details without requiring every internal symbol to carry an EXPORT macro, and enables
# before/after comparison of library/executable sizes and link/load times.
#
# Usage (in the same CMakeLists.txt that defines <target>):
#   phlex_make_internal_library(<target> LIBRARIES [PUBLIC ...] [PRIVATE ...])
#
# The LIBRARIES arguments mirror those of the original cet_make_library call but may
# substitute other _internal targets for the corresponding public ones so that the
# full transitive symbol set is visible.
function(phlex_make_internal_library target)
  cmake_parse_arguments(ARG "" "" "LIBRARIES" ${ARGN})

  set(internal "${target}_internal")

  # Retrieve sources and source directory from the public target so we don't
  # have to maintain a separate source list.
  get_target_property(srcs ${target} SOURCES)
  if(NOT srcs)
    message(FATAL_ERROR "phlex_make_internal_library: ${target} has no SOURCES property")
  endif()
  get_target_property(src_dir ${target} SOURCE_DIR)
  get_target_property(bin_dir ${target} BINARY_DIR)

  # Convert relative paths to absolute. Generated sources (e.g. configure_file
  # output) live in the binary directory rather than the source directory.
  set(abs_srcs "")
  foreach(s IN LISTS srcs)
    if(IS_ABSOLUTE "${s}")
      list(APPEND abs_srcs "${s}")
    elseif(EXISTS "${src_dir}/${s}")
      list(APPEND abs_srcs "${src_dir}/${s}")
    else()
      list(APPEND abs_srcs "${bin_dir}/${s}")
    endif()
  endforeach()

  # Use add_library directly (not cet_make_library) so that cetmodules does not
  # register this target for installation or package export.
  add_library(${internal} SHARED ${abs_srcs})

  if(ARG_LIBRARIES)
    target_link_libraries(${internal} ${ARG_LIBRARIES})
  endif()

  # Cetmodules automatically adds $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}> for
  # libraries it manages; replicate that here so consumers (e.g. layer_generator_internal)
  # can resolve project headers such as #include "phlex/core/...".
  # The _export.hpp headers live in PROJECT_BINARY_DIR/include/phlex.
  # Without CXX_VISIBILITY_PRESET hidden the export macros expand to the default
  # visibility attribute, making every symbol visible — exactly what we want here.
  target_include_directories(
    ${internal}
    PUBLIC
      "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
      "$<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include>"
  )

  # Propagate compile definitions and options that the public target carries
  # (e.g. BOOST_DLL_USE_STD_FS for run_phlex) so the internal build is equivalent.
  get_target_property(defs ${target} COMPILE_DEFINITIONS)
  if(defs)
    target_compile_definitions(${internal} PRIVATE ${defs})
  endif()

  get_target_property(opts ${target} COMPILE_OPTIONS)
  if(opts)
    target_compile_options(${internal} PRIVATE ${opts})
  endif()
endfunction()
