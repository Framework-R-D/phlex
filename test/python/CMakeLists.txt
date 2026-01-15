find_package(Python 3.12 COMPONENTS Interpreter REQUIRED)

# Verify installation of necessary python modules for specific tests

function(check_python_module_version MODULE_NAME MIN_VERSION OUT_VAR)
  execute_process(
    COMMAND
      ${Python_EXECUTABLE} -c
      "import sys
try:
    import ${MODULE_NAME}
    installed_version = getattr(${MODULE_NAME}, '__version__', None)
    if not installed_version:
        sys.exit(2)

    def parse(v):
        return tuple(map(int, v.split('.')[:3]))

    if parse(installed_version) >= parse('${MIN_VERSION}'):
        sys.exit(0)
    else:
        sys.exit(2)  # Version too low
except ImportError:
    sys.exit(1)
except Exception:
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
      message(WARNING "ENABLE_COVERAGE is ON but pytest-cov not found; Python coverage disabled")
    endif()
  endif()

  # tests of the python support modules (relies on cppyy)
  add_test(NAME py:phlex COMMAND ${PYTEST_COMMAND} WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

  set_property(TEST py:phlex PROPERTY ENVIRONMENT "PHLEX_INSTALL=${PYTHON_TEST_PHLEX_INSTALL}")
endif()

set(ACTIVE_PY_CPHLEX_TESTS "")

# numpy support if installed
if(HAS_NUMPY)
  # phlex-based tests that require numpy support
  add_test(NAME py:vec COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pyvec.jsonnet)
  list(APPEND ACTIVE_PY_CPHLEX_TESTS py:vec)

  add_test(NAME py:vectypes COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pyvectypes.jsonnet)
  list(APPEND ACTIVE_PY_CPHLEX_TESTS py:vectypes)

  add_test(NAME py:callback3 COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pycallback3.jsonnet)
  list(APPEND ACTIVE_PY_CPHLEX_TESTS py:callback3)

  # Expect failure for these tests (check for error propagation and type checking)
  add_test(NAME py:raise COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pyraise.jsonnet)
  set_tests_properties(
    py:raise
    PROPERTIES PASS_REGULAR_EXPRESSION "RuntimeError: Intentional failure"
  )
  list(APPEND ACTIVE_PY_CPHLEX_TESTS py:raise)

  add_test(NAME py:badbool COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pybadbool.jsonnet)
  set_tests_properties(
    py:badbool
    PROPERTIES
      PASS_REGULAR_EXPRESSION
        "Python conversion error for type bool: boolean value should be bool, or integer 1 or 0"
  )
  list(APPEND ACTIVE_PY_CPHLEX_TESTS py:badbool)

  add_test(NAME py:badint COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pybadint.jsonnet)
  set_tests_properties(
    py:badint
    PROPERTIES
      PASS_REGULAR_EXPRESSION
        "Python conversion error for type long: int/long conversion expects an integer object"
  )
  list(APPEND ACTIVE_PY_CPHLEX_TESTS py:badint)

  add_test(NAME py:baduint COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pybaduint.jsonnet)
  set_tests_properties(
    py:baduint
    PROPERTIES
      PASS_REGULAR_EXPRESSION
        "Python conversion error for type uint: can't convert negative value to unsigned long"
  )
  list(APPEND ACTIVE_PY_CPHLEX_TESTS py:baduint)

  add_test(
    NAME py:mismatch_ann
    COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pymismatch_annotations.jsonnet
  )
  set_tests_properties(
    py:mismatch_ann
    PROPERTIES
      PASS_REGULAR_EXPRESSION "number of inputs .* does not match number of annotation types"
  )
  list(APPEND ACTIVE_PY_CPHLEX_TESTS py:mismatch_ann)

  add_test(NAME py:veclists COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pyveclists.jsonnet)
  list(APPEND ACTIVE_PY_CPHLEX_TESTS py:veclists)

  add_test(NAME py:types COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pytypes.jsonnet)
  list(APPEND ACTIVE_PY_CPHLEX_TESTS py:types)
endif()

# C++ helper to provide a driver
add_library(cppsource4py MODULE source.cpp)
target_link_libraries(cppsource4py PRIVATE phlex::module)

# phlex-based tests (no cppyy dependency)
add_test(NAME py:add COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pyadd.jsonnet)
list(APPEND ACTIVE_PY_CPHLEX_TESTS py:add)

add_test(NAME py:config COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pyconfig.jsonnet)
list(APPEND ACTIVE_PY_CPHLEX_TESTS py:config)

add_test(NAME py:reduce COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pyreduce.jsonnet)
list(APPEND ACTIVE_PY_CPHLEX_TESTS py:reduce)

add_test(NAME py:coverage COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pycoverage.jsonnet)
list(APPEND ACTIVE_PY_CPHLEX_TESTS py:coverage)

add_test(
  NAME py:mismatch
  COMMAND ${PROJECT_BINARY_DIR}/bin/phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pymismatch.jsonnet
)
set_tests_properties(
  py:mismatch
  PROPERTIES
    PASS_REGULAR_EXPRESSION "number of inputs .* does not match number of annotation types"
)
list(APPEND ACTIVE_PY_CPHLEX_TESTS py:mismatch)

# "failing" tests for checking error paths
add_test(
  NAME py:failure
  COMMAND ${PROJECT_BINARY_DIR}/bin/phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pyfailure.jsonnet
)
set_tests_properties(
  py:failure
  PROPERTIES PASS_REGULAR_EXPRESSION "property \"input\" does not exist"
)
list(APPEND ACTIVE_PY_CPHLEX_TESTS py:failure)

set(TEST_PYTHONPATH ${CMAKE_CURRENT_SOURCE_DIR})
# Always add site-packages to PYTHONPATH for tests, as embedded python might
# not find them especially in spack environments where they are in
# non-standard locations
if(Python_SITELIB)
  set(TEST_PYTHONPATH ${TEST_PYTHONPATH}:${Python_SITELIB})
endif()
if(Python_SITEARCH AND NOT "${Python_SITEARCH}" STREQUAL "${Python_SITELIB}")
  set(TEST_PYTHONPATH ${TEST_PYTHONPATH}:${Python_SITEARCH})
endif()

if(DEFINED ENV{VIRTUAL_ENV})
  # Keep this for backward compatibility or if it adds something else
endif()
set(TEST_PYTHONPATH ${TEST_PYTHONPATH}:$ENV{PYTHONPATH})

# Environment variables required:
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
