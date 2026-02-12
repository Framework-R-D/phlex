#[================================================================[.rst:
ProcessSmc
----------

Define the function :command:`process_smc` to generate C++ source code
from an `SMC <https://smc.sourceforge.net/>`_ :file:`.sm` file.

#]================================================================]

# Avoid unwanted repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

find_package(Smc 6.0.1 REQUIRED)

#[================================================================[.rst:
.. command:: process_smc

   Generate C++ source code from an `SMC
   <https://smc.sourceforge.net/>`_ :file:`.sm` file.

   .. code-block:: cmake

      process_smc(<target-or-var> [<options>])

   .. seealso:: :module:`FindSmc`

   Options
   ^^^^^^^

   ``NO_INSTALL``
     Do not install generated files in either the include or source
     areas of the built package.

   .. versionadded:: 3.23.00

      ``NO_INSTALL_SOURCE``
        Do not install generated files in the source areas of the built
        package.

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<target-or-var>``
     If ``<target-or-var>`` is a target, generated sources will be added
     as ``PRIVATE`` sources via :command:`target_sources()
     <cmake-ref-current:command:target_sources>`.

     .. deprecated:: 2.10.00

        Otherwise, add the generated sources to the CMake variable
        ``<target-or-var>`` in the caller's scope.`

#]================================================================]

include(CetPackagePath)
include(InstallHeaders)
include(InstallSource)

function(process_smc TARGET_OR_VAR)
  cmake_parse_arguments(
    PARSE_ARGV 1 PSMC "NO_INSTALL;NO_INSTALL_SOURCE" "OUTPUT_DIR" ""
    )
  if(NOT PSMC_OUTPUT_DIR)
    set(PSMC_OUTPUT_DIR .)
  endif()
  list(TRANSFORM PSMC_UNPARSED_ARGUMENTS
       REPLACE "^(.*/)?(.*).sm$" "\\2_sm.cpp" OUTPUT_VARIABLE SMC_CPP_OUTPUTS
       )
  list(TRANSFORM PSMC_UNPARSED_ARGUMENTS REPLACE "^(.*/)?(.*).sm$" "\\2_sm.h"
                                                 OUTPUT_VARIABLE SMC_H_OUTPUTS
       )
  list(TRANSFORM PSMC_UNPARSED_ARGUMENTS
       REPLACE "^(.*/)?(.*).sm$" "\\2_sm.dot" OUTPUT_VARIABLE SMC_DOT_OUTPUTS
       )
  set(smc_cmd ${Java_JAVA_EXECUTABLE} -jar "$<TARGET_FILE:Smc::Smc>")
  cet_package_path(pkgpath PATH "${PSMC_OUTPUT_DIR}" BINARY)
  file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/${PSMC_OUTPUT_DIR}")
  foreach(
    source
    cpp_out
    h_out
    dot_out
    IN
    ZIP_LISTS
    PSMC_UNPARSED_ARGUMENTS
    SMC_CPP_OUTPUTS
    SMC_H_OUTPUTS
    SMC_DOT_OUTPUTS
    )
    cmake_path(ABSOLUTE_PATH source OUTPUT_VARIABLE abs_source)
    add_custom_command(
      OUTPUT "${PSMC_OUTPUT_DIR}/${cpp_out}" "${PSMC_OUTPUT_DIR}/${h_out}"
      COMMAND ${smc_cmd} -d "${pkgpath}" -c++ "${abs_source}"
      COMMAND
        perl -wapi~ -e
        "s&(#\\s*include\\s+)<statemap\\.h>&\${1}\"${pkgpath}/statemap.h\"&"
        "${pkgpath}/${h_out}"
      WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
      VERBATIM
      COMMENT "Generating C++ files from state machine ${source}"
      MAIN_DEPENDENCY "${source}"
      DEPENDS Smc::Smc
      )
    add_custom_command(
      OUTPUT "${PSMC_OUTPUT_DIR}/${dot_out}"
      COMMAND ${smc_cmd} -d "${PSMC_OUTPUT_DIR}" -graph -glevel 2
              "${abs_source}"
      COMMENT "Generating Graphviz output from state machine ${source}"
      MAIN_DEPENDENCY "${source}"
      DEPENDS Smc::Smc
      )
    string(
      MAKE_C_IDENTIFIER "${CETMODULES_CURRENT_PROJECT}/${pkgpath}/${dot_out}"
                        dot_target
      )
    add_custom_target(
      ${dot_target} ALL
      DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/${PSMC_OUTPUT_DIR}/${dot_out}"
      )
  endforeach()
  configure_file("${Smc_statemap_h}" "${PSMC_OUTPUT_DIR}/" COPYONLY)
  if(NOT PSMC_NO_INSTALL)
    list(APPEND SMC_H_OUTPUTS "statemap.h")
    list(TRANSFORM SMC_H_OUTPUTS PREPEND "${PROJECT_BINARY_DIR}/${pkgpath}/")
    list(TRANSFORM SMC_CPP_OUTPUTS PREPEND "${PROJECT_BINARY_DIR}/${pkgpath}/")
    install_headers(LIST ${SMC_H_OUTPUTS})
    if(NOT PSMC_NO_INSTALL_SOURCE)
      install_source(LIST ${SMC_H_OUTPUTS} ${SMC_CPP_OUTPUTS})
    endif()
  endif()
  set_source_files_properties(
    ${SMC_CPP_OUTPUTS} PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter"
    )
  if(TARGET ${TARGET_OR_VAR})
    target_sources(${TARGET_OR_VAR} PRIVATE ${SMC_CPP_OUTPUTS})
  else()
    message(FATAL_ERROR "${TARGET_OR_VAR} must be a valid target")
  endif()
endfunction()
