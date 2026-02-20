include(GenerateExportHeader)

function(phlex_apply_symbol_visibility target)
  set(EXPORT_HEADER "${PROJECT_BINARY_DIR}/include/${target}_export.hpp")
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
    ${target}
    PUBLIC $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}/include> $<INSTALL_INTERFACE:include>
  )

  install(FILES "${EXPORT_HEADER}" DESTINATION include)
endfunction()
