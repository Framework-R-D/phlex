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

# Register the scripts unit-test suite with CTest so it runs alongside all
# other tests.  The test file discovers tests automatically via pytest
# collection; the sys.path manipulation inside the package makes the scripts/
# directory importable without extra PYTHONPATH.
add_test(
  NAME scripts:unit
  COMMAND ${Python_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR} -v
  WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
)

# Coverage target for the scripts/ utility modules.  Mirrors the approach used
# by test/python/CMakeLists.txt for the coverage-python target.
if(HAS_SCRIPTS_PYTEST_COV AND ENABLE_COVERAGE)
  add_custom_target(
    coverage-scripts
    COMMAND
      ${Python_EXECUTABLE} -m pytest ${CMAKE_CURRENT_SOURCE_DIR}
      --cov=${PROJECT_SOURCE_DIR}/scripts --omit=${CMAKE_CURRENT_SOURCE_DIR}/*
      --cov-report=term-missing --cov-report=xml:${CMAKE_BINARY_DIR}/coverage-scripts.xml
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    COMMENT "Running scripts/ Python coverage report"
    VERBATIM
  )
  message(STATUS "Added 'coverage-scripts' target for scripts/ utility modules.")
elseif(ENABLE_COVERAGE AND NOT HAS_SCRIPTS_PYTEST_COV)
  message(WARNING "ENABLE_COVERAGE is ON but pytest-cov not found; scripts coverage disabled.")
endif()
