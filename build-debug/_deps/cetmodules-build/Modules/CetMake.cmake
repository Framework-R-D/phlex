#[================================================================[.rst:
CetMake
-------

``CetMake.cmake`` defines several commands for specifying build
operations in CMake:


* :command:`cet_make_exec`
* :command:`cet_script`

#]================================================================]

# Avoid unwanted repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

include(CetCMakeUtils)
include(CetCopy)
include(CetMakeLibrary)
include(CetPackagePath)
include(CetProcessLiblist)
include(CetRegisterExportSet)

set(_cet_make_exec_usage "")

#[================================================================[.rst:
.. command:: cet_make_exec

   Make an executable.

   .. seealso:: :command:`add_executable()
                <cmake-ref-current:command:add_executable>`.

   .. code-block:: cmake

      cet_make_exec(NAME <exec-name> [<options>])

   Options
   ^^^^^^^

   ``EXCLUDE_FROM_ALL``
     Set the :prop_tgt:`EXCLUDE_FROM_ALL
     <cmake-ref-current:prop_tgt:EXCLUDE_FROM_ALL>` property on the
     executable target.

   ``EXEC_NAME <exec-name>``
     .. deprecated:: 2.10.00 use ``NAME <exec-name> instead``

   ``EXPORT_SET <export-set>``
     The executable will be exported as part of the specified
     :external+cmake-ref-current:ref:`export set <install(export)>`.

   ``LIBRARIES <library-specification> ...``
     Library dependencies (passed to :command:`target_link_libraries()
     <cmake-ref-current:command:target_link_libraries>`).

   ``LOCAL_INCLUDE_DIRS <dir> ...``
     Specify local include directories.

   ``NAME <exec-name>``
     The built executable shall be named ``<exec-name>``

   ``NO_EXPORT``
     The executable target will not be exported or installed.

   ``NO_EXPORT_ALL_SYMBOLS``
     Disable the default addition of `-rdynamic
     <https://gcc.gnu.org/onlinedocs/gcc/Link-Options.html#index-rdynamic>`_
     or equivalent to the executable link stage.

   ``NO_INSTALL``
     Synonym for ``NO_EXPORT``.

   ``NOP``
     Option / argument disambiguator; no other function.

   ``SOURCE <source> ...``
     Source files to be compiled to produce the executable.

   ``USE_BOOST_UNIT``
     The executable uses `Boost unit test functions
     <https://www.boost.org/doc/libs/release/libs/test/doc/html/index.html>`_
     and should be compiled and linked accordingly.

   ``USE_CATCH2_MAIN``
     The executable uses a generic `Catch2
     <https://github.com/catchorg/Catch2>`_ ``main()`` function and
     should be compiled and linked accordingly.

   ``USE_CATCH_MAIN``
     .. deprecated:: 2.10.00 use ``USE_CATCH2_MAIN``

#]================================================================]

function(cet_make_exec)
  cmake_parse_arguments(
    PARSE_ARGV
    0
    CME
    "EXCLUDE_FROM_ALL;NO_EXPORT;NO_EXPORT_ALL_SYMBOLS;NO_INSTALL;NOP;USE_BOOST_UNIT;USE_CATCH_MAIN;USE_CATCH2_MAIN"
    "EXEC_NAME;EXPORT_SET;NAME"
    "LIBRARIES;LOCAL_INCLUDE_DIRS;SOURCE"
    )
  # Argument verification.
  if(CME_EXEC_NAME)
    warn_deprecated("EXEC_NAME" NEW "NAME")
    set(CME_NAME "${CME_EXEC_NAME}")
    unset(CME_EXEC_NAME)
  elseif(NOT CME_NAME)
    warn_deprecated(
      "<name> as non-option argument to cet_make_exec()" NEW
      "cet_make_exec(NAME <name> ...)"
      )
    list(POP_FRONT CME_UNPARSED_ARGUMENTS CME_NAME)
  endif()
  if(NOT CME_NAME)
    message(FATAL_ERROR "NAME <name> *must* be provided")
  elseif(CME_UNPARSED_ARGUMENTS)
    message(
      FATAL_ERROR "non-option arguments prohibited: ${CME_UNPARSED_ARGUMENTS}"
      )
  endif()
  if(CME_USE_CATCH_MAIN)
    warn_deprecated("cet_make_exec(): USE_CATCH_MAIN" NEW "USE_CATCH2_MAIN")
    set(CME_USE_CATCH2_MAIN TRUE)
    unset(CME_USE_CATCH_MAIN)
  endif()
  if(NOT DEFINED CME_SOURCE)
    cet_source_file_extensions(source_glob)
    list(TRANSFORM source_glob PREPEND "${CME_NAME}.")
    file(GLOB found_sources CONFIGURE_DEPENDS ${source_glob})
    list(LENGTH found_sources n_sources)
    if(n_sources EQUAL 1)
      list(POP_FRONT found_sources CME_SOURCE)
    elseif(n_sources EQUAL 0)
      message(
        WARNING
          "no suitable candidate source found for ${CME_NAME} from \
enabled languages ${_cet_enabled_languages} in ${CMAKE_CURRENT_SOURCE_DIR}.
If this is intentional, specify with dangling SOURCE keyword to silence this warning\
"
        )
    else()
      message(
        FATAL_ERROR
          "unable to identify a unique candidate source for ${CME_NAME} - found:"
          "\n${found_sources}\nUse SOURCE <sources> to remove ambiguity"
        )
    endif()
  endif()
  cet_passthrough(FLAG IN_PLACE CME_EXCLUDE_FROM_ALL)
  # Define the main executable target.
  add_executable(${CME_NAME} ${CME_EXCLUDE_FROM_ALL} ${CME_SOURCE})
  if(CME_EXCLUDE_FROM_ALL)
    set_target_properties(
      ${CME_NAME} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD TRUE
      )
  endif()
  # Local include directories.
  if(NOT (DEFINED CME_LOCAL_INCLUDE_DIRS OR "LOCAL_INCLUDE_DIRS" IN_LIST
                                            CME_KEYWORDS_MISSING_VALUES)
     )
    set(CME_LOCAL_INCLUDE_DIRS "${CETMODULES_CURRENT_PROJECT_BINARY_DIR}"
                               "${CETMODULES_CURRENT_PROJECT_SOURCE_DIR}"
        )
  endif()
  target_include_directories(${CME_NAME} PRIVATE ${CME_LOCAL_INCLUDE_DIRS})
  # Handle Boost unit test framework.
  if(CME_USE_BOOST_UNIT)
    find_package(
      Boost QUIET
      COMPONENTS unit_test_framework
      REQUIRED
      )
    if(TARGET Boost::unit_test_framework AND Boost_VERSION
                                             VERSION_GREATER_EQUAL 1.70.0
       )
      target_link_libraries(${CME_NAME} PRIVATE Boost::unit_test_framework)
      # Belt and braces (cf historical bug in fhiclcpp tests).
      target_compile_definitions(${CME_NAME} PRIVATE BOOST_TEST_NO_OLD_TOOLS)
    else()
      # *Someone* didn't use Boost's CMake config file to define targets.
      target_link_libraries(
        ${CME_NAME} PRIVATE ${Boost_UNIT_TEST_FRAMEWORK_LIBRARY}
        )
      target_compile_definitions(
        ${CME_NAME} PRIVATE "BOOST_TEST_MAIN;BOOST_TEST_DYN_LINK"
        )
    endif()
  endif()
  # Handle request for Catch2 main.
  if(CME_USE_CATCH2_MAIN)
    find_package(Catch2 QUIET REQUIRED)
    if(3.0 VERSION_GREATER ${Catch2_VERSION}) # Old.
      set(Catch2_main_target Catch2_main)
      if(NOT TARGET ${Catch2_main_target})
        cet_localize_pv(cetmodules CATCH2_MAIN)
        get_property(
          catch2_include_dir
          TARGET Catch2::Catch2
          PROPERTY INTERFACE_INCLUDE_DIRECTORIES
          )
        list(POP_FRONT catch2_include_dir catch2_include_subdir)
        if(EXISTS "${catch2_include_subdir}/catch2")
          set(catch2_include_subdir "catch2")
        else()
          set(catch2_include_subdir "catch")
        endif()
        cet_make_library(
          LIBRARY_NAME
          Catch2_main
          STATIC
          STRIP_LIBS
          EXCLUDE_FROM_ALL
          NO_INSTALL
          SOURCE
          ${cetmodules_CATCH2_MAIN}
          LIBRARIES
          PUBLIC
          Catch2::Catch2
          )
        target_compile_definitions(
          Catch2_main
          PRIVATE "CET_CATCH2_INCLUDE_SUBDIR=${catch2_include_subdir}"
          )
      endif()
    else() # New
      set(Catch2_main_target Catch2::Catch2WithMain)
    endif()
    target_link_libraries(${CME_NAME} PRIVATE ${Catch2_main_target})
  endif()
  if(NOT CME_NO_EXPORT_ALL_SYMBOLS)
    target_link_options(${CME_NAME} PRIVATE -rdynamic)
  endif()
  # Library links.
  cet_process_liblist(liblist ${CME_NAME} PRIVATE ${CME_LIBRARIES})
  target_link_libraries(${CME_NAME} ${liblist})
  # For target aliases.
  cet_register_export_set(
    SET_NAME
    ${CME_EXPORT_SET}
    SET_VAR
    CME_EXPORT_SET
    NAMESPACE_VAR
    namespace
    NO_REDEFINE
    )
  # ############################################################################
  # Installation.
  if(NOT CME_NO_INSTALL)
    install(
      TARGETS ${CME_NAME}
      EXPORT ${CME_EXPORT_SET}
      RUNTIME DESTINATION ${${CETMODULES_CURRENT_PROJECT_NAME}_BIN_DIR}
      )
    if(NOT CME_NO_EXPORT)
      _add_to_exported_targets(EXPORT_SET ${CME_EXPORT_SET} TARGETS ${CME_NAME})
    endif()
  endif()
  add_executable(${namespace}::${CME_NAME} ALIAS ${CME_NAME})
  foreach(alias IN LISTS CME_ALIAS)
    if(CME_NO_INSTALL OR CME_NO_EXPORT)
      add_executable(${namespace}::${alias} ALIAS ${CME_NAME})
    else()
      cet_make_alias(
        NAME
        ${alias}
        EXPORT_SET
        ${CME_EXPORT_SET}
        TARGET
        ${CME_NAME}
        TARGET_EXPORT_SET
        ${CME_EXPORT_SET}
        )
    endif()
  endforeach()
endfunction()

#[================================================================[.rst:
.. command:: cet_script

   Install the named scripts.

   .. code-block:: cmake

      cet_script([<options>] <script> ...)

   Options
   ^^^^^^^

   ``ALWAYS_COPY``
     If specified, scripts will be copied to
     :variable:`CMAKE_RUNTIME_OUTPUT_DIRECTORY
     <cmake-ref-current:variable:CMAKE_RUNTIME_OUTPUT_DIRECTORY>` at
     build time in addition to being installed. Otherwise, scripts will
     only be copied if they need to be made executable prior to
     installation.

   ``DEPENDENCIES <dep> ...``
     If ``<dep>`` changes, ``<script>`` shall be considered out-of-date.

   ``DESTINATION <dir>``
     Specify the installation directory (default
     :variable:`\<PROJECT-NAME>_SCRIPTS_DIR`).

   ``EXPORT_SET <export-set>``
     Scripts will be exported as part of the specified
     :external+cmake-ref-current:ref:`export set <install(export)>`.

   ``GENERATED``
     .. deprecated:: 2.10.00

        Redundant—added automatically by :command:`add_custom_command()
        <cmake-ref-current:command:add_custom_command>`.

   ``NO_EXPORT``
     The scripts shall not be exported as targets.

   ``NO_INSTALL``
     The scripts shall not be installed; implies ``NO_EXPORT``.

   ``NOP``
     Option / argument disambiguator; no other function.

   ``ONLY_QUALIFIED_TARGET``
     .. versionadded:: 3.27.00

        Create only a qualified name-based target (`namespace::target`)
        for the source, rather than both qualified and unqualified
        targets.

   ``REMOVE_EXTENSIONS``
     Extensions will be removed from script names when they are
     installed.

#]================================================================]

function(cet_script)
  cmake_parse_arguments(
    PARSE_ARGV
    0
    CS
    "ALWAYS_COPY;GENERATED;NO_EXPORT;NO_INSTALL;NOP;ONLY_QUALIFIED_TARGET;REMOVE_EXTENSIONS"
    "DESTINATION;EXPORT_SET"
    "DEPENDENCIES"
    )
  if(CS_GENERATED)
    warn_deprecated(
      "cet_script(GENERATED)"
      " - CMake source property GENERATED is set automatically by add_custom_command, etc. or can be set manually otherwise"
      )
  endif()
  if(NOT CS_DESTINATION)
    set(CS_DESTINATION "${${CETMODULES_CURRENT_PROJECT_NAME}_SCRIPTS_DIR}")
  endif()
  foreach(script IN LISTS CS_UNPARSED_ARGUMENTS)
    unset(need_copy)
    get_property(
      generated
      SOURCE "${script}"
      PROPERTY GENERATED
      )
    get_filename_component(script_name "${script}" NAME)
    if(generated OR CS_GENERATED)
      get_filename_component(
        script_source "${script}" ABSOLUTE BASE_DIR
        "${CMAKE_CURRENT_BINARY_DIR}"
        )
    else()
      # Should exist: make sure we can find it and that it is executable.
      get_filename_component(script_source "${script}" ABSOLUTE)
      if(NOT EXISTS "${script_source}")
        message(
          FATAL_ERROR
            "${script} is not accessible: correct location or set GENERATED source property"
          )
      endif()
      if(CS_ALWAYS_COPY)
        set(need_copy TRUE)
      else()
        execute_process(
          COMMAND test -x "${script_source}"
          OUTPUT_QUIET ERROR_QUIET
          RESULT_VARIABLE need_copy
          )
      endif()
    endif()
    if(CS_REMOVE_EXTENSIONS)
      get_filename_component(target "${script_source}" NAME_WE)
      cet_passthrough(KEYWORD RENAME target rename_arg)
    else()
      set(target "${script_name}")
    endif()
    cet_register_export_set(
      SET_NAME
      ${CS_EXPORT_SET}
      SET_VAR
      CS_EXPORT_SET
      NAMESPACE_VAR
      ns
      NO_REDEFINE
      )
    if(need_copy)
      if(NOT CS_ALWAYS_COPY)
        message(
          WARNING
            "${script} is not executable: copying to ${CMAKE_RUNTIME_OUTPUT_DIRECTORY} as PROGRAM"
          )
      endif()
      if(CS_ONLY_QUALIFIED_TARGET)
        set(nat)
      else()
        set(nat NAME_AS_TARGET)
      endif()
      cet_copy(
        "${script_source}"
        PROGRAMS
        NAME
        ${target}
        ${nat}
        DEPENDENCIES
        ${CS_DEPENDENCIES}
        DESTINATION
        "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        )
      # cet_copy() will create the primary (non-qualified) target as a custom
      # target. We create the namespaced target as IMPORTED rather than ALIAS
      # because one can only create ALIASes to libraries, executables or
      # IMPORTED targets.
      add_executable(${ns}::${target} IMPORTED GLOBAL)
      set_target_properties(
        ${ns}::${target}
        PROPERTIES IMPORTED_LOCATION
                   "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}"
        )
    else()
      # "Normal" - non-qualified target is an IMPORTED reference to the source
      # file, and the namespaced target is an ALIAS for transparency between
      # standalone and multi-project (e.g. MRB) builds.
      if(CS_ONLY_QUALIFIED_TARGET)
        add_executable(${ns}::${target} IMPORTED GLOBAL)
        set_target_properties(
          ${ns}::${target} PROPERTIES IMPORTED_LOCATION "${script_source}"
          )
      else()
        add_executable(${target} IMPORTED GLOBAL)
        set_target_properties(
          ${target} PROPERTIES IMPORTED_LOCATION "${script_source}"
          )
        add_executable(${ns}::${target} ALIAS ${target})
      endif()
    endif()
    if(NOT CS_NO_INSTALL)
      install(
        PROGRAMS "${script_source}"
        DESTINATION "${CS_DESTINATION}"
        ${rename_arg}
        )
      if(NOT CS_NO_EXPORT)
        _cet_export_import_cmd(
          TARGETS
          ${ns}::${target}
          COMMANDS
          "\
add_executable(${ns}::${target} IMPORTED)
set_target_properties(${ns}::${target}
  PROPERTIES IMPORTED_LOCATION \"\${PACKAGE_PREFIX_DIR}/${CS_DESTINATION}/${target}\")\
"
          )
      endif()
    endif()
  endforeach()
endfunction()
