find_package(Python 3.12 COMPONENTS Interpreter REQUIRED)

# Detect pytest and pytest-cov availability for coverage reporting.
function(_check_scripts_pytest_module MODULE_NAME OUT_VAR)
  execute_process(
    COMMAND ${Python_EXECUTABLE} -c "import ${MODULE_NAME}"
    RESULT_VARIABLE _result
    OUTPUT_QUIET
    ERROR_QUIET
  )
  if(_result EQUAL 0)
    set(${OUT_VAR} TRUE PARENT_SCOPE)
  else()
    set(${OUT_VAR} FALSE PARENT_SCOPE)
  endif()
endfunction()

_check_scripts_pytest_module("pytest" HAS_SCRIPTS_PYTEST)
_check_scripts_pytest_module("pytest_cov" HAS_SCRIPTS_PYTEST_COV)

if(NOT HAS_SCRIPTS_PYTEST)
  message(WARNING "pytest not found; scripts/test/ tests will not be registered with CTest.")
  return()
endif()

# Register each test module separately so CTest can run the scripts suite in
# parallel. The sys.path manipulation inside the package makes scripts/
# importable without extra PYTHONPATH.
set(
  scripts_test_modules
  test_check_codeql_alerts
  test_clang_tidy_check_summary
  test_clang_tidy_diff_issues
  test_clang_tidy_fixes_to_problems
  test_codeql_reset_dismissed_alerts
  test_coverage_scripts
  test_devcontainer_host
  test_fix_header_guards
  test_promote_phlex_dependencies
  test_sarif_alerts
  test_upgrade_repos
  test_verification
  test_workflow_cleanup
)
foreach(test_name IN LISTS scripts_test_modules)
  string(REGEX REPLACE "^test_" "" test_name_without_prefix "${test_name}")
  add_test(
    NAME "scripts:${test_name_without_prefix}"
    COMMAND ${Python_EXECUTABLE} -m pytest "${CMAKE_CURRENT_SOURCE_DIR}/${test_name}.py" -v
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
  )
endforeach()

# Coverage target for Python utilities in scripts/, ci/, and .devcontainer/.
if(HAS_SCRIPTS_PYTEST_COV AND ENABLE_COVERAGE)
  add_custom_target(
    coverage-scripts
    COMMAND
      ${Python_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}
      --cov=${PROJECT_SOURCE_DIR}/scripts --cov=${PROJECT_SOURCE_DIR}/ci
      --cov=${PROJECT_SOURCE_DIR}/.devcontainer --omit=${CMAKE_CURRENT_SOURCE_DIR}/*
      --cov-report=term-missing --cov-report=xml:${CMAKE_BINARY_DIR}/coverage-scripts.xml
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Running scripts, CI, and devcontainer Python coverage report"
    VERBATIM
  )
  message(STATUS "Added 'coverage-scripts' target for repository Python utilities.")
elseif(ENABLE_COVERAGE AND NOT HAS_SCRIPTS_PYTEST_COV)
  message(WARNING "ENABLE_COVERAGE is ON but pytest-cov not found; scripts coverage disabled.")
endif()
