find_package(Python 3.12 COMPONENTS Interpreter Development NumPy REQUIRED)

if(Python_NumPy_VERSION VERSION_LESS "2.0.0")
  message(
    FATAL_ERROR
    "NumPy version is too low: ${Python_NumPy_VERSION} found, at least 2.0.0 required"
  )
endif()

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

install(
  DIRECTORY python/phlex
  DESTINATION lib/python${Python_VERSION_MAJOR}.${Python_VERSION_MINOR}/site-packages
)
