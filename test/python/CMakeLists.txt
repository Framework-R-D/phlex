find_package(
  Python 3.12
  COMPONENTS Interpreter Development
  QUIET
  )

if(Python_FOUND)

  # Verify installation of necessary python modules for specific tests

  function(check_python_module_version MODULE_NAME MIN_VERSION OUT_VAR)
    execute_process(
      COMMAND
        ${Python_EXECUTABLE} -c "import sys
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
      set(${OUT_VAR}
          TRUE
          PARENT_SCOPE
          )
    elseif(_module_check_result EQUAL 1)
      set(${OUT_VAR}
          FALSE
          PARENT_SCOPE
          ) # silent b/c common
    elseif(_module_check_result EQUAL 2)
      message(
        WARNING
          "Python module '${MODULE_NAME}' found but version too low (min required: ${MIN_VERSION})."
        )
      set(${OUT_VAR}
          FALSE
          PARENT_SCOPE
          )
    else()
      message(
        WARNING "Unknown error while checking Python module '${MODULE_NAME}'."
        )
      set(${OUT_VAR}
          FALSE
          PARENT_SCOPE
          )
    endif()
  endfunction()

  check_python_module_version("cppyy" "3.6.0" HAS_CPPYY)
  check_python_module_version("numba" "0.61.0" HAS_NUMBA)
  check_python_module_version("numpy" "2.0.0" HAS_NUMPY)

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

    # tests of the python support modules (relies on cppyy)
    add_test(
      NAME py:phlex
      COMMAND ${PYTHON_TEST_EXECUTABLE} -m pytest test_phlex.py
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      )

    set_property(
      TEST py:phlex PROPERTY ENVIRONMENT
                             "PHLEX_INSTALL=${PYTHON_TEST_PHLEX_INSTALL}"
      )

  endif()

  # C++ helper to provide a driver
  add_library(py_cppdriver MODULE driver.cpp)
  target_link_libraries(py_cppdriver PRIVATE Boost::json phlex::core)

  # Phlex module to run Python algorithms
  add_library(
    pymodule MODULE pymodule.cpp modulewrap.cpp configwrap.cpp lifelinewrap.cpp
                    errorwrap.cpp
    )
  include_directories(pymodule, ${Python_INCLUDE_DIRS})
  target_link_libraries(
    pymodule
    PRIVATE phlex::module ${Python_LIBRARIES}
    PUBLIC Python::Python
    )

  # numpy support if installed
  if(HAS_NUMPY)

    # locate numpy's header directory
    execute_process(
      COMMAND "${Python_EXECUTABLE}" -c
              "import numpy; print(numpy.get_include(), end='')"
      RESULT_VARIABLE NUMPY_RESULT
      OUTPUT_VARIABLE NUMPY_INCLUDE_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE
      )

    if(NUMPY_RESULT EQUAL 0)
      include_directories(pymodule PRIVATE ${NUMPY_INCLUDE_DIR})
      target_compile_definitions(
        pymodule PRIVATE PHLEX_HAVE_NUMPY=1
                         NPY_NO_DEPRECATED_API=NPY_1_7_API_VERSION
        )
    endif()

    # phlex-based tests that require numpy support
    add_test(NAME py:vec
             COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pyvec.jsonnet -g
                     pyvec.gv
             )
  endif()

  # phlex-based tests (no cppyy dependency)
  add_test(NAME py:add
           COMMAND phlex -c ${CMAKE_CURRENT_SOURCE_DIR}/pyadd.jsonnet -g
                   pyadd.gv
           )

  set(TEST_PYTHONPATH ${CMAKE_CURRENT_SOURCE_DIR})
  if(DEFINED ENV{VIRTUAL_ENV})
    set(TEST_PYTHONPATH ${TEST_PYTHONPATH}:${Python_SITELIB}:${Python_SITEARCH})
  endif()
  set(TEST_PYTHONPATH ${TEST_PYTHONPATH}:$ENV{PYTHONPATH})

  set_tests_properties(
    py:add py:vec
    PROPERTIES
      ENVIRONMENT
      "SPDLOG_LEVEL=debug;PHLEX_PLUGIN_PATH=${CMAKE_CURRENT_BINARY_DIR};PYTHONPATH=${TEST_PYTHONPATH};PHLEX_INSTALL=${PYTHON_TEST_PHLEX_INSTALL}"
    )
endif() # Python available
