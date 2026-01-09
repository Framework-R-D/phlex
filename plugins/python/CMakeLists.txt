find_package(Python 3.12 COMPONENTS Interpreter Development QUIET)

if(Python_FOUND)
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

  check_python_module_version("numpy" "2.0.0" HAS_NUMPY)

  # Phlex module to run Python algorithms
  add_library(
    pymodule
    MODULE
    src/pymodule.cpp
    src/modulewrap.cpp
    src/configwrap.cpp
    src/lifelinewrap.cpp
    src/errorwrap.cpp
  )
  include_directories(pymodule, ${Python_INCLUDE_DIRS})
  target_link_libraries(pymodule PRIVATE phlex::module ${Python_LIBRARIES} PUBLIC Python::Python)

  install(TARGETS pymodule LIBRARY DESTINATION lib)

  # numpy support if installed
  if(HAS_NUMPY)
    # locate numpy's header directory
    execute_process(
      COMMAND "${Python_EXECUTABLE}" -c "import numpy; print(numpy.get_include(), end='')"
      RESULT_VARIABLE NUMPY_RESULT
      OUTPUT_VARIABLE NUMPY_INCLUDE_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(NUMPY_RESULT EQUAL 0)
      include_directories(pymodule PRIVATE ${NUMPY_INCLUDE_DIR})
      target_compile_definitions(
        pymodule
        PRIVATE PHLEX_HAVE_NUMPY=1 NPY_NO_DEPRECATED_API=NPY_1_7_API_VERSION
      )
    endif()
  endif()
endif() # Python available
