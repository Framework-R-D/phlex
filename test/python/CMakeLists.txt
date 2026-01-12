find_package(Python 3.12 COMPONENTS Interpreter REQUIRED)

# Verify installation of necessary python modules for specific tests

function(check_python_module_version MODULE_NAME MIN_VERSION OUT_VAR)
  execute_process(
    COMMAND
      ${Python_EXECUTABLE} -c
      "import sys
try:
    import ${MODULE_NAME}
    from packaging.version import parse as parse_version
    installed_version = getattr(${MODULE_NAME}, '__version__', None)
    if parse_version(installed_version) >= parse_version('${MIN_VERSION}'):
        sys.exit(0)
    else:
        sys.exit(2)  # Version too low
except ImportError:
    sys.exit(1)"
    RESULT_VARIABLE _module_check_result
  )

  if(_module_check_result EQUAL 0)
    set(${OUT_VAR} TRUE PARENT_SCOPE)
  elseif(_module_check_result EQUAL 1)
    set(${OUT_VAR} FALSE PARENT_SCOPE) # silent b/c common
  elseif(_module_check_result EQUAL 2)
    message(
      WARNING
      "Python module '${MODULE_NAME}' found but version too low (min required: ${MIN_VERSION})."
    )
    set(${OUT_VAR} FALSE PARENT_SCOPE)
  else()
    message(WARNING "Unknown error while checking Python module '${MODULE_NAME}'.")
    set(${OUT_VAR} FALSE PARENT_SCOPE)
  endif()
endfunction()

check_python_module_version("cppyy" "3.6.0" HAS_CPPYY)
check_python_module_version("numba" "0.61.0" HAS_NUMBA)
check_python_module_version("numpy" "2.0.0" HAS_NUMPY)
check_python_module_version("pytest" "7.0.0" HAS_PYTEST)
check_python_module_version("pytest_cov" "4.0.0" HAS_PYTEST_COV)

if(HAS_CPPYY)
  # Explicitly define Python executable variable to ensure it is visible in
  # the test environment
  set(PYTHON_TEST_EXECUTABLE ${Python_EXECUTABLE})

  # Export the location of phlex include headers
  if(DEFINED ENV{PHLEX_INSTALL})
    set(PYTHON_TEST_PHLEX_INSTALL $ENV{PHLEX_INSTALL})
  else()
    set(PYTHON_TEST_PHLEX_INSTALL ${CMAKE_SOURCE_DIR})
  endif()

  # Determine pytest command based on coverage support
  if(HAS_PYTEST_COV AND ENABLE_COVERAGE)
    set(
      PYTEST_COMMAND
      ${PYTHON_TEST_EXECUTABLE}
      -m
      pytest
      --cov=${CMAKE_CURRENT_SOURCE_DIR}
      --cov-report=term-missing
      --cov-report=xml:${CMAKE_BINARY_DIR}/coverage-python.xml
      --cov-report=html:${CMAKE_BINARY_DIR}/coverage-python-html
      test_phlex.py
    )
    message(STATUS "Python tests will run with coverage reporting (pytest-cov)")
  else()
    set(PYTEST_COMMAND ${PYTHON_TEST_EXECUTABLE} -m pytest test_phlex.py)
    if(ENABLE_COVERAGE AND NOT HAS_PYTEST_COV)
      message(
        WARNING "ENABLE_COVERAGE is ON but pytest-cov not found; Python coverage disabled"
      )
    endif()
  endif()

  # tests of the python support modules (relies on cppyy)
  add_test(
    NAME py:phlex
    COMMAND ${PYTEST_COMMAND}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  )

  set_property(TEST py:phlex PROPERTY ENVIRONMENT "PHLEX_INSTALL=${PYTHON_TEST_PHLEX_INSTALL}")
endif()

set(ACTIVE_PY_CPHLEX_TESTS "")

# C++ helper to provide a driver
add_library(cppsource4py MODULE source.cpp)
target_link_libraries(cppsource4py PRIVATE phlex::module)

# phlex-based tests that require numpy support
add_test(NAME py:vec COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pyvec.jsonnet)
list(APPEND ACTIVE_PY_CPHLEX_TESTS py:vec)

# phlex-based tests (no cppyy dependency)
add_test(NAME py:add COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pyadd.jsonnet)
list(APPEND ACTIVE_PY_CPHLEX_TESTS py:add)

add_test(NAME py:config COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pyconfig.jsonnet)
list(APPEND ACTIVE_PY_CPHLEX_TESTS py:config)

add_test(NAME py:reduce COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pyreduce.jsonnet)
list(APPEND ACTIVE_PY_CPHLEX_TESTS py:reduce)

# "failing" tests for checking error paths
add_test(
  NAME py:failure
  COMMAND
    ${CMAKE_CURRENT_SOURCE_DIR}/failing_test_wrap.sh ${PROJECT_BINARY_DIR}/bin/phlex -c
    ${CMAKE_CURRENT_SOURCE_DIR}/pyfailure.jsonnet
)
set_tests_properties(
  py:failure
  PROPERTIES PASS_REGULAR_EXPRESSION "property \"input\" does not exist"
)
list(APPEND ACTIVE_PY_CPHLEX_TESTS py:failure)

# Environment variables required:
set(TEST_PYTHONPATH ${CMAKE_CURRENT_SOURCE_DIR}:$ENV{PYTHONPATH})
set(
  PYTHON_TEST_ENVIRONMENT
  "SPDLOG_LEVEL=debug;PHLEX_PLUGIN_PATH=${PROJECT_BINARY_DIR};PYTHONPATH=${TEST_PYTHONPATH};PHLEX_INSTALL=${PYTHON_TEST_PHLEX_INSTALL}"
)

set_tests_properties(${ACTIVE_PY_CPHLEX_TESTS} PROPERTIES ENVIRONMENT "${PYTHON_TEST_ENVIRONMENT}")

# phlex-based test to ensure that sys.path is sanely set for virtual environment
set(PY_VIRTUAL_ENV_DIR ${CMAKE_CURRENT_BINARY_DIR}/py_virtual_env)
execute_process(COMMAND python -m venv ${PY_VIRTUAL_ENV_DIR})
configure_file(pysyspath.jsonnet.in pysyspath.jsonnet @ONLY)
add_test(NAME py:syspath COMMAND phlex -c ${CMAKE_CURRENT_BINARY_DIR}/pysyspath.jsonnet)

# Activate the Python virtual environment "by hand".  Requires setting the VIRTUAL_ENV
# environment variable and prepending to the PATH environment variable.
set_tests_properties(
  py:syspath
  PROPERTIES
    ENVIRONMENT "${PYTHON_TEST_ENVIRONMENT};VIRTUAL_ENV=${PY_VIRTUAL_ENV_DIR}"
    ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${PY_VIRTUAL_ENV_DIR}/bin"
)
