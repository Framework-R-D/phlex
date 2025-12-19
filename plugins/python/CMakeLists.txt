find_package(Python 3.12 COMPONENTS Interpreter Development NumPy REQUIRED)

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
target_link_libraries(pymodule PRIVATE phlex::module Python::Python Python::NumPy)
target_compile_definitions(pymodule PRIVATE NPY_NO_DEPRECATED_API=NPY_1_7_API_VERSION)

install(TARGETS pymodule LIBRARY DESTINATION lib)
