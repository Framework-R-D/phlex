#[================================================================[.rst:
InstallPython
-------------

Define function :command:`install_python` to (optionally generate and)
invoke a Python ``distutils`` :file:`setup.py` script.

#]================================================================]

# Avoid unwanted repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

#[================================================================[.rst:
.. command:: install_python

   Invoke Python ``distutils`` functionality to install Python files.

   .. parsed-literal::

      install_python(`SETUP_PY`_ <setup.py> [NO_INSTALL])
      install_python([`GENERATE_SETUP_PY`_] [NO_INSTALL] [<gen-options>])

   .. signature:: install_python(SETUP_PY <setup.py> [NO_INSTALL])

      Invoke ``<setup.py>`` to build and install Python package files.

   .. signature:: install_python(GENERATE_SETUP_PY [NO_INSTALL] [<gen-options>])

      Generate a ``setup.py`` file with information provided by
      ``<gen-options>``:

      ``DATA_FILES DIR <dir> <file> ...``
        Specify ``<file> ...` for inclusion in the package in
        non-package subdirectory ``<dir>``.

      .. _install_python-MODULES-opt:

      ``MODULES <module> ...``
        Specify single Python files for use as modules (with
        import). They will be installed in ``lib/``

      ``NAME <name>``
        The name of the Python package to be installed (default
        :variable:`CETMODULES_CURRENT_PROJECT_NAME`).

      ``PACKAGES <package> ...``
        Specify packages to be installed under ``lib/``, with
        ``pkg1.pkg2`` installed in :file:`pkg1/pkg2`.

      ``PACKAGE_DATA  { ROOT | PKG <pkg> } <file> ...``
        Install non-Python ``<file> ...`` in the top level package
        (``ROOT``) or with package ``<pkg>``.

      ``SCRIPTS <script> ...``
        Install executable Python script ``<script> ...`` in ``bin/``

        If required, the same file may be both a script and a module.

        .. seealso:: :ref:`install_python-MODULES-opt`

      ``SETUP_ARGS <arg> ...``
        Pass ``<arg> ...`` to the ``distutils`` ``setup()`` command, if
        required.

      ``SETUP_PREAMBLE <preamble>``
        Place ``<preamble>`` at the beginning of the generated
        :file:`setup.py` file prior to the invocation of ``setup()``.

      ``VERSION <version>``
        Specify the package version (default
        :variable:`CETMODULES_CURRENT_PROJECT_VERSION`).

   Example
   ^^^^^^^

   For a directory containing the following files:

   * :file:`README.txt`
   * :file:`test.py`
   * :file:`test2.py`
   * :file:`test3.py`
   * :file:`pkg1/README-pkg1.txt`
   * :file:`pkg1/__init__.py`
   * :file:`pkg1/bill.py`
   * :file:`pkg1/pkg2/README-pkg2.txt`
   * :file:`pkg1/pkg2/__init__.py`
   * :file:`pkg1/pkg2/fred.py`

   The command:

   .. code-block:: cmake

      install_python(SCRIPTS test.py test2.py test3.py
                     MODULES test2
                     PACKAGES pkg1 pkg1.pkg2
                     PACKAGE_DATA ROOT README.txt PKG pkg1 README-pkg1.txt
                     DATA_FILES
                     DIR doc README.txt pkg1/README-pkg1.txt pkg1/pkg2/README-pkg2.txt
                     DIR etc README.txt pkg1/README-pkg1.txt pkg1/pkg2/README-pkg2.txt)

   will produce the following directory structure under
   :variable:`CETMODULES_CURRENT_PROJECT_BINARY_DIR`:

   * :file:`bin/test.py`
   * :file:`bin/test2.py`
   * :file:`bin/test3.py`
   * :file:`doc/README.txt`
   * :file:`doc/README-pkg1.txt`
   * :file:`doc/README-pkg2.txt`
   * :file:`etc/README.txt`
   * :file:`etc/README-pkg1.txt`
   * :file:`etc/README-pkg2.txt`
   * :file:`lib/art-1.09.02-py2.7.egg-info`
   * :file:`lib/pkg1/__init__.py`
   * :file:`lib/pkg1/__init__.pyc`
   * :file:`lib/pkg1/bill.py`
   * :file:`lib/pkg1/bill.pyc`
   * :file:`lib/pkg1/pkg2/__init__.py`
   * :file:`lib/pkg1/pkg2/__init__.pyc`
   * :file:`lib/pkg1/pkg2/fred.py`
   * :file:`lib/pkg1/pkg2/fred.pyc`
   * :file:`lib/pkg1/README-pkg1.txt`
   * :file:`lib/test2.py`
   * :file:`lib/test2.pyc`

   After install, the same files will be visible under the corresponding
   directory in the installed product.

#]================================================================]

function(install_python)
  set(setup_arg_indent "        ")
  cmake_parse_arguments(
    PARSE_ARGV 0 IP "NO_INSTALL" "SETUP_PY;NAME;VERSION"
    "SETUP_ARGS;SETUP_PREAMBLE;SCRIPTS;MODULES;PACKAGES;PACKAGE_DATA;DATA_FILES"
    )
  if(NOT IP_SCRIPTS
     AND NOT IP_MODULES
     AND NOT IP_SETUP_PY
     AND NOT PACKAGES
     )
    message(FATAL_ERROR "install_python called with no defined "
                        "SCRIPTS, MODULES, PACKAGES or SETUP_PY."
            )
  endif()
  if(IP_SETUP_PY)
    foreach(
      var IN
      ITEMS NAME
            VERSION
            SETUP_ARGS
            SETUP_PREAMBLE
            SCRIPTS
            MODULES
            PACKAGES
            PACKGE_DATA
            DATA_FILES
      )
      if(IP_${var})
        list(APPEND error_vars "${var}")
      endif()
    endforeach()
    if(error_vars)
      message(
        FATAL_ERROR
          "install_python: specification of ${error_vars} not valid with SETUP_PY."
          "makes no sense."
        )
    endif()
  endif()
  if(IP_PACKAGE_DATA AND NOT IP_PACKAGES)
    message(
      FATAL_ERROR
        "install_python: PACKAGE_DATA makes no sense without PACKAGES."
      )
  endif()
  if(NOT IP_NAME)
    set(IP_NAME ${CETMODULES_CURRENT_PROJECT_NAME})
  endif() # IP_NAME
  if(NOT IP_VERSION)
    set(IP_VERSION ${CETMODULES_CURRENT_PROJECT_VERSION})
  endif() # IP_VERSION
  if(IP_SCRIPTS) # scripts=[ ... ]
    foreach(item IN LISTS IP_SCRIPTS)
      list(APPEND tmp "${CMAKE_CURRENT_SOURCE_DIR}/${item}")
    endforeach()
    set(IP_SCRIPTS "${tmp}")
    _to_python_list(scripts PYTHON_VAR scripts ${IP_SCRIPTS})
    list(APPEND IP_SETUP_ARGS "${scripts}")
  endif() # IP_SCRIPTS
  if(IP_MODULES) # modules=[ ... ]
    _to_python_list(modules PYTHON_VAR py_modules ${IP_MODULES})
    list(APPEND IP_SETUP_ARGS "${modules}")
  endif() # IP_MODULES
  if(IP_PACKAGES) # packages=[ ... ]
    _to_python_list(packages PYTHON_VAR packages ${IP_PACKAGES})
    list(APPEND IP_SETUP_ARGS "${packages}")
  endif() # IP_PACKAGES
  unset(this_list)
  if(IP_PACKAGE_DATA) # package_data={ '' : [ ... ] '<pkg>' [ ... ] ... }
    foreach(arg IN LISTS IP_PACKAGE_DATA)
      if(arg STREQUAL "ROOT" OR arg STREQUAL "PKG")
        if(NOT package_data)
          set(package_data "package_data={ ")
        endif()
        if(this_list) # Already have a list to process.
          set(package_data "${package_data}'${this_pkg}' : ")
          _to_python_list(package_data APPEND ${this_list})
          set(package_data
              "${package_data},\n${setup_arg_indent}               "
              )
          unset(this_list)
        endif()
        if(arg STREQUAL "ROOT")
          set(this_pkg "")
        elseif(arg STREQUAL "PKG")
          set(new_pkg TRUE)
        endif()
      elseif(new_pkg) # Package name.
        set(this_pkg "${arg}")
        unset(new_pkg)
      else() # Just a list element.
        if(NOT package_data) # Error
          message(
            FATAL_ERROR
              "install_python: PACKAGE_DATA first argument must be ROOT or PKG."
            )
        endif()
        list(APPEND this_list "${arg}")
      endif()
    endforeach()
    if(this_list) # One last list to process.
      set(package_data "${package_data}'${this_pkg}' : ")
      _to_python_list(package_data APPEND ${this_list})
    endif()
    set(package_data "${package_data} }")
    list(APPEND IP_SETUP_ARGS "${package_data}")
  endif() # IP_PACKAGE_DATA
  unset(this_list)
  if(IP_DATA_FILES) # data_files=[ ('<dir>', [ ... ]), ... ]
    foreach(arg IN LISTS IP_DATA_FILES)
      if(arg STREQUAL "DIR")
        if(NOT data_files)
          set(data_files "data_files=[ ")
        endif()
        if(this_list) # Already have a list to process.
          set(data_files "${data_files}('${this_dir}', ")
          _to_python_list(data_files APPEND ${this_list})
          set(data_files "${data_files}),\n${setup_arg_indent}             ")
          unset(this_list)
        endif()
        set(new_dir TRUE)
      elseif(new_dir) # Dir name.
        set(this_dir "${arg}")
        unset(new_dir)
      else() # Just a list element.
        if(NOT data_files) # Error
          message(
            FATAL_ERROR "install_python: DATA_FILES first argument must be DIR."
            )
        endif()
        list(APPEND this_list "${CMAKE_CURRENT_SOURCE_DIR}/${arg}")
      endif()
    endforeach()
    if(this_list) # One last list to process.
      set(data_files "${data_files}('${this_dir}', ")
      _to_python_list(data_files APPEND ${this_list})
      set(data_files "${data_files})")
    endif()
    set(data_files "${data_files} ]")
    list(APPEND IP_SETUP_ARGS "${data_files}")
  endif() # IP_DATA_FILES
  if(NOT IP_SETUP_PY)
    set(IP_SETUP_PY "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
    if(IP_SETUP_PREAMBLE)
      file(WRITE "${IP_SETUP_PY}" ${IP_SETUP_PREAMBLE})
    else()
      file(WRITE "${IP_SETUP_PY}"
           "from distutils.core import setup, Extension\n"
           )
    endif()
    set(setup_items
        "name='${IP_NAME}'" "version='${IP_VERSION}'"
        "package_dir={ '' : '${CMAKE_CURRENT_SOURCE_DIR}' }" "${IP_SETUP_ARGS}"
        )
    string(REPLACE ";" ",\n${setup_arg_indent}" setup_arg_string
                   "${setup_items}"
           )
    file(APPEND "${IP_SETUP_PY}" "\n\n" "if __name__ == '__main__':\n"
                                 "  setup(${setup_arg_string})\n"
         )
  endif()
  add_custom_target(
    python_${IP_NAME}_build ALL
    COMMAND
      python "${IP_SETUP_PY}" install
      --install-lib="${CETMODULES_CURRENT_PROJECT_BINARY_DIR}/${${CETMODULES_CURRENT_PROJECT_NAME}_LIBRARY_DIR}"
      --install-scripts="${CETMODULES_CURRENT_PROJECT_BINARY_DIR}/${${CETMODULES_CURRENT_PROJECT_NAME}_SCRIPTS_DIR}"
      --install-headers="${CETMODULES_CURRENT_PROJECT_NAME}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    DEPENDS "${IP_SETUP_PY}"
    )
  if(NOT IP_NO_INSTALL)
    install(
      CODE "execute_process(COMMAND python \"${IP_SETUP_PY}\" install --prefix=\"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\" --install-lib=\"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${${CETMODULES_CURRENT_PROJECT_NAME}_LIBRARY_DIR}\" --install-scripts=\"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${${CETMODULES_CURRENT_PROJECT_NAME}_SCRIPTS_DIR}\" --install-headers=\"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${${CETMODULES_CURRENT_PROJECT_NAME}_INCLUDE_DIR}\")"
      )
  endif()
endfunction()

function(_to_python_list OUTPUT_VAR)
  cmake_parse_arguments(PARSE_ARGV 1 TPL "APPEND" "PYTHON_VAR" "")
  if(TPL_PYTHON_VAR)
    set(tmp "${TPL_PYTHON_VAR}=[ ")
  else()
    set(tmp "[ ")
  endif()
  foreach(item IN LISTS TPL_UNPARSED_ARGUMENTS)
    if(NOT item OR item MATCHES [=[[^-+0-9.]*([eE][-0-9]+)?]=])
      set(item "'${item}'")
    endif()
    set(tmp "${tmp}${item},")
  endforeach()
  string(REGEX REPLACE [[(,| )$]] " ]" tmp "${tmp}")
  if(TPL_APPEND)
    set(${OUTPUT_VAR}
        "${${OUTPUT_VAR}}${tmp}"
        PARENT_SCOPE
        )
  else()
    set(${OUTPUT_VAR}
        "${tmp}"
        PARENT_SCOPE
        )
  endif()
endfunction()
