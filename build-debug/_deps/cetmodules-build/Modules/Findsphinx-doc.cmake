#[================================================================[.rst:
X
-
#]================================================================]
find_package(
  Python3
  COMPONENTS Interpreter
  REQUIRED
  )
get_property(
  _find_sphinx_python3
  TARGET Python3::Interpreter
  PROPERTY LOCATION
  )
execute_process(
  COMMAND ${_find_sphinx_python3} -c "import sphinx; print(sphinx.__path__[0]);"
  OUTPUT_STRIP_TRAILING_WHITESPACE
  OUTPUT_VARIABLE _find_sphinx_path
  ERROR_QUIET
  )
unset(_find_sphinx_python3)
string(REGEX REPLACE "^(.*)/lib/.*$" "\\1/bin" _find_sphinx_path
                     "${_find_sphinx_path}"
       )
find_program(
  SPHINX-BUILD_EXECUTABLE sphinx-build
  HINTS ${_find_sphinx_path}
  DOC "Location of sphinx-build documentation compiler"
  )
unset(_find_sphinx_path)
mark_as_advanced(SPHINX-BUILD_EXECUTABLE)

if(SPHINX-BUILD_EXECUTABLE)
  if(NOT TARGET sphinx-doc::sphinx-build)
    add_executable(sphinx-doc::sphinx-build IMPORTED)
    set_target_properties(
      sphinx-doc::sphinx-build PROPERTIES IMPORTED_LOCATION
                                          "${SPHINX-BUILD_EXECUTABLE}"
      )
  endif()
  message(VERBOSE "SPHINX-BUILD_EXECUTABLE=${SPHINX-BUILD_EXECUTABLE}")

  execute_process(
    COMMAND "${SPHINX-BUILD_EXECUTABLE}" --version
    ERROR_QUIET
    OUTPUT_VARIABLE sphinx-doc_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  string(REGEX REPLACE "^[^ 	]+[ 	]+" "" sphinx-doc_VERSION
                       "${sphinx-doc_VERSION}"
         )
  message(VERBOSE "found sphinx-doc_VERSION=${sphinx-doc_VERSION}")
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(
  sphinx-doc
  VERSION_VAR sphinx-doc_VERSION
  REQUIRED_VARS SPHINX-BUILD_EXECUTABLE
  )
