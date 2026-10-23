# FIXME: Replace this implementation with cetmodules COMPILE_ONLY tests once its
# data-file list quoting is fixed.
set(fold_accumulator_only_diagnostic "at_least_two_input_parameters")
set(
  fold_resource_only_diagnostic
  "A fold requires at least one data product in addition to the accumulator"
)
foreach(test_name IN ITEMS fold_accumulator_only fold_resource_only)
  add_library(${test_name}_compile OBJECT EXCLUDE_FROM_ALL ${test_name}_test.cpp)
  # Omit intentionally invalid sources from the compilation database so
  # run-clang-tidy does not analyze them and report expected compiler errors.
  set_target_properties(${test_name}_compile PROPERTIES EXPORT_COMPILE_COMMANDS OFF)
  target_link_libraries(${test_name}_compile PRIVATE phlex::core)
  add_test(
    NAME ${test_name}
    COMMAND
      ${CMAKE_COMMAND} "-DBUILD_DIR=${CMAKE_BINARY_DIR}" "-DBUILD_CONFIG=$<CONFIG>"
      "-DBUILD_TARGET=${test_name}_compile" "-DEXPECTED_DIAGNOSTIC=${${test_name}_diagnostic}" -P
      "${CMAKE_CURRENT_SOURCE_DIR}/check_compile_failure.cmake"
  )
  # Each test invokes the build system in the same build directory.
  set_tests_properties(
    ${test_name}
    PROPERTIES RUN_SERIAL TRUE LABELS "DEFAULT;RELEASE;COMPILE_ONLY"
  )
endforeach()
