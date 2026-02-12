#[================================================================[.rst:
CetTest
-------

  This module defines the function :command:`cet_test` to specify tests,
  and the related utility functions :command:`cet_test_env`, and
  :command:`cet_test_assertion`.

#]================================================================]

# Avoid unnecessary repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.19...4.1 FATAL_ERROR)

# For warn_deprecated()
include(CetCMakeUtils)
# Copy function
include(CetCopy)
# Need cet_script for PREBUILT scripts
include(CetMake)
# To ascertain the current subdirectory within a package
include(CetPackagePath)
# May need to escape a string to avoid misinterpretation as regex
include(CetRegexEscape)
# Need to specify transitive BINARY_DIR paths for COMPILE_ONLY tests
include(CetTransitivePaths)

# ##############################################################################
# Programs and Modules

# Default comparator
set(CET_RUNANDCOMPARE "${CMAKE_CURRENT_LIST_DIR}/RunAndCompare.cmake")

# Test run wrapper
set(CET_TEST_WRAPPER cetmodules::cet_exec_test)

# Properties
define_property(
  TEST
  PROPERTY KEYWORDS
  BRIEF_DOCS "Keywords describing the test engine used, test type, etc."
  FULL_DOCS "ETHEL"
  )
# ##############################################################################

#[================================================================[.rst:
.. command:: cet_test

   Define a test target for execution by :manual:`ctest(1)
   <cmake-ref-current:manual:ctest(1)>`

   .. seealso:: :command:`add_test() <cmake-ref-current:command:add_test>`.

   .. parsed-literal::

      cet_test([`BUILD_EXECUTABLE`_] :ref:`\<target> <cet_test-target>` :ref:`\<build-options> <cet_test-build-options>` [:ref:`\<install-options> <cet_test-install-options>`]
               [:ref:`NO_AUTO <cet_test-NO_AUTO-opt>`\|\ :ref:`\<test-options> <cet_test-test-options>`])
      cet_test(`COMPILE_ONLY`_ :ref:`\<target> <cet_test-target>` :ref:`\<build-options> <cet_test-build-options>` [:ref:`NO_AUTO <cet_test-NO_AUTO-opt>`\|\ :ref:`\<test-options> <cet_test-test-options>`])
      cet_test(`HANDBUILT`_ :ref:`\<target> <cet_test-target>` [:ref:`\<install-options> <cet_test-install-options>`] [:ref:`NO_AUTO <cet_test-NO_AUTO-opt>`\|\ :ref:`\<test-options> <cet_test-test-options>`])
      cet_test(`PREBUILT`_ :ref:`\<target> <cet_test-target>` [:ref:`DEPENDENCIES \<dep-target> ... <cet_test-DEPENDENCIES-opt>`] [:ref:`\<install-options> <cet_test-install-options>`]
               [:ref:`NO_AUTO <cet_test-NO_AUTO-opt>`\|\ :ref:`\<test-options> <cet_test-test-options>`])

   .. signature::
      cet_test(BUILD_EXECUTABLE <target> <build-options> [...])

      .. parsed-literal::

         cet_test([BUILD_EXECUTABLE] :ref:`\<target> <cet_test-target>` :ref:`\<build-options> <cet_test-build-options>` [:ref:`\<install-options> <cet_test-install-options>`]
                  [:ref:`NO_AUTO <cet_test-NO_AUTO-opt>`\|\ :ref:`\<test-options> <cet_test-test-options>`])

      Build the test executable with specified ``build-options`` in
      addition to configuring its invocation as a test.

   .. signature::
      cet_test(COMPILE_ONLY <target> <build-options> [...])

      .. versionadded:: 3.21.00

      .. parsed-literal::

         cet_test(COMPILE_ONLY :ref:`\<target> <cet_test-target>` :ref:`\<build-options> <cet_test-build-options>` [:ref:`NO_AUTO <cet_test-NO_AUTO-opt>`\|\ :ref:`\<test-options> <cet_test-test-options>`])

      Configure a test to compile and link—but not run—an executable.

      .. note::

         * In the event of a configuration failure of the generated
           project, the test will fail regardless of ``WILL_FAIL``
           status with a message:

           .. code-block:: console

              Error regular expression found in output. Regex=[<<ERROR: build system failure for COMPILE_ONLY test>>
              ]

         * The test compilation is implemented as a separate
           configuration/build of a generated, dedicated project as part
           of the test run by :manual:`ctest(1)
           <cmake-ref-current:manual:ctest(1)>`.

         * The project invoking ``COMPILE_ONLY`` tests is located by the
           generated project with :command:`find_package()
           <cmake-ref-current:command:find_package>`: the project must
           therefore be capable of being used in this way without being
           installed. :command:`cet_cmake_config` generates suitable
           configuration for use from within the build tree, but any
           path variables defined therein must resolve without requiring
           the package be installed.

         * If compilation failure is required for test success, the test
           designer is responsible for preventing unwanted test success
           due to compilation failure for reasons other than those
           expected.

      .. rst-class:: text-start

         .. seealso::

            :prop_test:`WILL_FAIL <cmake-ref-current:prop_test:WILL_FAIL>`,
            :prop_test:`PASS_REGULAR_EXPRESSION <cmake-ref-current:prop_test:PASS_REGULAR_EXPRESSION>`,
            :prop_test:`FAIL_REGULAR_EXPRESSION <cmake-ref-current:prop_test:FAIL_REGULAR_EXPRESSION>`,
            :prop_test:`SKIP_REGULAR_EXPRESSION <cmake-ref-current:prop_test:SKIP_REGULAR_EXPRESSION>`.

   .. signature::
      cet_test(PREBUILT <target> [...])

      .. parsed-literal::

         cet_test(PREBUILT :ref:`\<target> <cet_test-target>` [:ref:`DEPENDENCIES \<dep-target> ... <cet_test-DEPENDENCIES-opt>`] [:ref:`\<install-options> <cet_test-install-options>`]
                  [:ref:`NO_AUTO <cet_test-NO_AUTO-opt>`\|\ :ref:`\<test-options> <cet_test-test-options>`])

      Configure a test to run a script.

   .. signature::
      cet_test(HANDBUILT <target> [...])

      .. parsed-literal::

         cet_test(HANDBUILT :ref:`\<target> <cet_test-target>` [:ref:`\<install-options> <cet_test-install-options>`] [:ref:`NO_AUTO <cet_test-NO_AUTO-opt>`\|\ :ref:`\<test-options> <cet_test-test-options>`])

      Configure a test to run an arbitrary executable.

   Options
   ^^^^^^^

   .. _cet_test-DEPENDENCIES-opt:

   ``DEPENDENCIES <dep-target> ...``
     List of top-level dependencies to consider for a ``PREBUILT``
     target. "Top-level" implies a target (not file) created with
     :command:`add_executable()
     <cmake-ref-current:command:add_executable>`,
     :command:`add_library() <cmake-ref-current:command:add_library>` or
     :command:`add_custom_target()
     <cmake-ref-current:command:add_custom_target>`.

     .. seealso::

        :command:`add_dependencies()
        <cmake-ref-current:command:add_dependencies>`

   .. _cet_test-NO_AUTO-opt:

   ``NO_AUTO``
     Do not configure the test(s) with :command:`add_test()
     <cmake-ref-current:command:add_test>`.

   .. _cet_test-build-options:

   Build options
   """""""""""""

   ``LIBRARIES <library-specification> ...``
     Library dependencies (passed to :command:`target_link_libraries()
     <cmake-ref-current:command:target_link_libraries>`).

   ``SOURCE <source> ...``
     Source files from which to build the test executable.

   ``SOURCES <source> ...``
     .. deprecated:: 2.10.00 use ``SOURCE``.

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

   .. _cet_test-install-options:

   Install options
   """""""""""""""

   ``EXPORT_SET <export-set>``
     The executable will be exported as part of the specified
     :external+cmake-ref-current:ref:`export set <install(export)>`.

   ``INSTALL_BIN``
     Install the test script/executable into the package's binary
     directory.

   ``INSTALL_EXAMPLE``
     Install the test's source and data files into the package's
     examples area.

   ``INSTALL_SOURCE``
     Install the source files for the test in the package's source area.

   ``NO_EXPORT``
     The executable target will not be exported or installed.

   .. _cet_test-test-options:

   Test options
   """"""""""""

   ``CONFIGURATIONS <config> ...``
     The test is valid for the specified CMake configuration(s)

   ``DATAFILES <file> ...``
     Input and/or reference output files to be copied to the test area in the
     build tree for use by the test. If there is no path, or a relative
     path, the file is assumed to be in or under
     :variable:`CMAKE_CURRENT_SOURCE_DIR
     <cmake-ref-current:variable:CMAKE_CURRENT_SOURCE_DIR>`.

   ``DIRTY_WORKDIR``
     If set, the working directory will not be cleared prior to
     execution of the test.

   ``NO_OPTIONAL_GROUPS``
     Do not apply any CMake test labels to the configured
     test(s). Default behavior is to add the labels, ``DEFAULT`` and
     ``RELEASE`` to each test.

   ``OPTIONAL_GROUPS <test-group> ...``
     Add the specified CMake test labels to the configured test(s).

   ``OUTPUT_FILTER <filter>``
     Specify a single filter for test output. Specify arguments to same
     with ``OUTPUT_FILTER_ARGS``. Mutually exclusive with
     ``OUTPUT_FILTERS``.

   ``OUTPUT_FILTERS "<filter [<filter-args>]>" ...``
     Specify one or more filters to apply sequentially to test
     output. Each specified filter with its arguments must be quoted as
     a single shell "word." Mutually exclusive with ``OUTPUT_FILTER``
     and ``OUTPUT_FILTER_ARGS``.

   ``OUTPUT_FILTER_ARGS <arg> ...``
     Specify arguments to ``<filter>`` as specified by
     ``OUTPUT_FILTER``. Mutually exclusive with ``OUTPUT_FILTERS``.

   ``PARG_<label> <opt>[=] <opt-val> ...``
     Specify a parameter axis ``<label>`` with values to configure a
     combinatoric family of tests. ``<label>`` must be unique within a
     single ``cet_test()`` invocation. If ``<opt>`` is specified with a
     trailing ``=`` then tests will be executed with arguments
     ``<opt>=<opt-val>`` rather than ``<opt> <opt-val>``.

     .. note::

        * Test target names will have ``_<num>`` appended, where
          ``<num>`` is zero-padded to ensure the same number of digits
          are appended to each target name.

        * Permuted arguments will *precede* ``TEST_ARGS``.

        * In the case of multiple ``PARG...`` options, permuted
          arguments will be combined linearly rather than
          multiplicatively, with shorter parameter lists being repeated
          cyclically as necessary.

   ``REF <output-ref> [<error-ref>]``
     Specify an output and optional error-output reference file with
     which to compare the (possibly filtered) output of the configured
     test(s). Incompatible with the CMake test properties
     :prop_test:`PASS_REGULAR_EXPRESSION
     <cmake-ref-current:prop_test:PASS_REGULAR_EXPRESSION>` and
     :prop_test:`FAIL_REGULAR_EXPRESSION
     <cmake-ref-current:prop_test:FAIL_REGULAR_EXPRESSION>`.

   ``REMOVE_ON_FAILURE <file-or-dir> ...``
     Upon ``TEST_EXEC`` failure, these files and/or directories shall be
     removed if they exist.

   ``REQUIRED_FILES <file> ...``
     These files are required to be present before the test will be
     executed. If any are missing, :manual:`ctest
     <cmake-ref-current:manual:ctest(1)>` will record ``NOT RUN`` for
     this test.

   .. _cet_test-REQUIRED_FIXTURES-opt:

   ``REQUIRED_FIXTURES <test-target> ...``
     Each specified ``<test-target>`` must be run prior to the test(s)
     currently being configured. If ``<test-target>`` is missing from
     the test selection for a given :manual:`ctest
     <cmake-ref-current:manual:ctest(1)>`, it will be added.

   .. _cet_test-REQUIRED_TESTS-opt:

   ``REQUIRED_TESTS <test-target> ...``
     As per ``REQUIRED_FIXTURES``, except that the test selection will
     only be amended if
     :variable:`\<PROJECT-NAME>_TEST_DEPS_AS_FIXTURES` is ``TRUE``.

   ``*SAN_OPTIONS <val>``
     Specify the desired value of the corresponding sanitizer control
     environment variable for the configured test(s).

   ``SCOPED``
     Test target names will have
     :variable:`CETMODULES_CURRENT_PROJECT_NAME`: prepended.

   ``TEST_ARGS <arg> ...``
     Specify arguments to the test executable for the configured
     test(s).

   ``TEST_EXEC <test-exec>``
     Specify the executable to be run by the configured test(s). Valid
     only for ``HANDBUILT`` tests.

   ``TEST_PROPERTIES <prop>=<val> ...``
     Properties to be added to the test.

     .. note::

        Properties must be properly escaped to avoid unwanted
        interpolation by CMake.

     .. seealso::

        :command:`set_tests_properties() <cmake-ref-current:command:set_tests_properties>`,
        :external+cmake-ref-current:ref:`CMake target properties <target properties>`.

   ``TEST_WORKDIR <dir>``
     Test to execute (and support files to be copied to) ``<dir>``. If
     not specified, :variable:`${CMAKE_CURRENT_BINARY_DIR}
     <cmake-ref-current:variable:CMAKE_CURRENT_BINARY_DIR>`\/\
     :ref:`\<target> <cet_test-target>`:file:`.d` will be created and used. If
     relative or not qualified, ``<dir>`` is assumed to be releative to
     :variable:`${CMAKE_CURRENT_BINARY_DIR}
     <cmake-ref-current:variable:CMAKE_CURRENT_BINARY_DIR>`.


   Non-option arguments
   """"""""""""""""""""

   .. _cet_test-target:

   ``<target>``
     The name of the test target, and/or the build target if one is
     generated.

#]================================================================]

function(cet_test CET_TARGET)
  if(NOT BUILD_TESTING) # See CMake's CTest module.
    return()
  endif()
  # Parse arguments.
  if(CET_TARGET MATCHES /)
    message(
      FATAL_ERROR
        "${CET_TARGET} should not be a path. Use a simple "
        "target name with the HANDBUILT and TEST_EXEC options instead."
      )
  endif()
  cmake_parse_arguments(
    PARSE_ARGV
    1
    CET
    "BUILD_EXECUTABLE;COMPILE_ONLY;DIRTY_WORKDIR;HANDBUILT;INSTALL_BIN;INSTALL_EXAMPLE;INSTALL_SOURCE;NO_AUTO;NO_EXPORT;NO_OPTIONAL_GROUPS;PREBUILT;SCOPED;USE_BOOST_UNIT;USE_CATCH2_MAIN;USE_CATCH_MAIN"
    "EXPORT_SET;OUTPUT_FILTER;TEST_EXEC;TEST_WORKDIR"
    "CONFIGURATIONS;DATAFILES;DEPENDENCIES;LIBRARIES;OPTIONAL_GROUPS;OUTPUT_FILTERS;OUTPUT_FILTER_ARGS;REF;REMOVE_ON_FAILURE;REQUIRED_FILES;REQUIRED_FIXTURES;REQUIRED_TESTS;SOURCE;SOURCES;TEST_ARGS;TEST_PROPERTIES"
    )
  if(CET_OUTPUT_FILTERS AND CET_OUTPUT_FILTER_ARGS)
    message(
      FATAL_ERROR
        "OUTPUT_FILTERS is incompatible with FILTER_ARGS:\nEither use the singular OUTPUT_FILTER or use double-quoted strings in OUTPUT_FILTERS\nE.g. OUTPUT_FILTERS \"filter1 -x -y\" \"filter2 -y -z\""
      )
  endif()

  # CET_SOURCES is obsolete.
  if(CET_SOURCES)
    warn_deprecated("cet_test(): SOURCES" NEW "SOURCE")
    list(APPEND CET_SOURCE ${CET_SOURCES})
    unset(CET_SOURCES)
  endif()

  # CET_USE_CATCH_MAIN is obsolete.
  if(CET_USE_CATCH_MAIN)
    warn_deprecated("cet_test(): USE_CATCH_MAIN" NEW "USE_CATCH2_MAIN")
    set(CET_USE_CATCH2_MAIN TRUE)
    unset(CET_USE_CATCH_MAIN)
  endif()

  if(CET_COMPILE_ONLY
     AND (CET_BUILD_EXECUTABLE
          OR CET_HANDBUILT
          OR CET_INSTALL_BIN
          OR CET_INSTALL_EXAMPLE
          OR CET_NO_AUTO
          OR CET_NO_EXPORT
          OR CET_PREBUILT
          OR CET_EXPORT_SET
          OR CET_TEST_EXEC
          OR CET_DATAFILES
          OR CET_REQUIRED_FILES
          OR CET_REQUIRED_FIXTURES
          OR CET_REQUIRED_TESTS
          OR CET_TEST_ARGS
          OR CET_TEST_WORKDIR
         )
     )
    message(
      FATAL_ERROR
        "COMPILE_ONLY is incompatible with the following options:
BUILD_EXECUTABLE
HANDBUILT
INSTALL_BIN
INSTALL_EXAMPLE
NO_AUTO
NO_EXPORT
PREBUILT
EXPORT_SET
TEST_EXEC
DATAFILES
REQUIRED_FILES
REQUIRED_FIXTURES
REQUIRED_TESTS
TEST_ARGS
TEST_WORKDIR"
      )
  endif()

  # For passthrough to cet_script, cet_make_exec, etc.
  set(exec_extra_args EXPORT_SET ${CET_EXPORT_SET} NOP)
  if(NOT CET_INSTALL_BIN)
    list(APPEND exec_extra_args NO_INSTALL)
  endif()
  cet_passthrough(FLAG APPEND CET_NO_EXPORT exec_extra_args)

  # Find any arguments related to permuted test arguments.
  foreach(OPT IN LISTS CET_UNPARSED_ARGUMENTS)
    if(OPT MATCHES [[^PARG_([A-Za-z_][A-Za-z0-9_]*)$]])
      if(OPT IN_LIST parg_option_names)
        message(
          SEND_ERROR
            "For test ${TEST_TARGET_NAME}, permuted argument label ${CMAKE_MATCH_1} specified multiple times."
          )
      endif()
      list(APPEND parg_option_names "${OPT}")
      list(APPEND parg_labels "${CMAKE_MATCH_1}")
    elseif(OPT MATCHES [[SAN_OPTIONS$]])
      if(OPT IN_LIST san_option_names)
        message(
          FATAL_ERROR
            "For test ${TEST_TARGET_NAME}, ${OPT} specified multiple times"
          )
      endif()
      list(APPEND san_option_names "${OPT}")
    endif()
  endforeach()
  set(cetp_list_options PERMUTE_OPTS ${parg_option_names})
  set(cetp_onearg_options PERMUTE ${san_option_names})
  cmake_parse_arguments(
    CETP "" "${cetp_onearg_options}" "${cetp_list_options}"
    "${CET_UNPARSED_ARGUMENTS}"
    )
  if(CETP_PERMUTE)
    message(
      FATAL_ERROR "PERMUTE is a keyword reserved for future functionality."
      )
  elseif(CETP_PERMUTE_OPTS)
    message(
      FATAL_ERROR "PERMUTE_OPTS is a keyword reserved for future functionality."
      )
  endif()
  list(LENGTH parg_labels NPARG_LABELS)
  _cet_process_pargs(NTESTS "${parg_labels}")
  if(CETP_UNPARSED_ARGUMENTS)
    message(
      FATAL_ERROR
        "cet_test: Unparsed (non-option) arguments detected: \"${CETP_UNPARSED_ARGUMENTS}.\" Check for missing keyword(s) in the definition of test ${CET_TARGET} in your CMakeLists.txt."
      )
  endif()

  if((CET_HANDBUILT AND CET_PREBUILT)
     OR (CET_HANDBUILT AND CET_USE_CATCH2_MAIN)
     OR (CET_PREBUILT AND CET_USE_CATCH2_MAIN)
     )
    # CET_HANDBUILT, CET_PREBUILT and CET_USE_CATCH2_MAIN are mutually
    # exclusive.
    message(
      FATAL_ERROR
        "cet_test: target ${CET_TARGET} must have only one of the"
        " CET_HANDBUILT, CET_PREBUILT, or CET_USE_CATCH2_MAIN options set."
      )
  elseif(CET_PREBUILT) # eg scripts.
    cet_script(
      ${CET_TARGET} ${exec_extra_args} DEPENDENCIES ${CET_DEPENDENCIES}
      )
  elseif(NOT CET_HANDBUILT) # Normal build, possibly with CET_USE_CATCH2_MAIN
                            # set.
    # Build the executable.
    if(NOT CET_SOURCE) # Useful default.
      set(CET_SOURCE ${CET_TARGET}.cc)
    endif()
    cet_passthrough(FLAG IN_PLACE CET_USE_BOOST_UNIT)
    cet_passthrough(FLAG IN_PLACE CET_USE_CATCH2_MAIN)
    if(NOT CET_COMPILE_ONLY)
      cet_make_exec(
        NAME
        ${CET_TARGET}
        ${exec_extra_args}
        ${CET_USE_BOOST_UNIT}
        ${CET_USE_CATCH2_MAIN}
        SOURCE
        ${CET_SOURCE}
        LIBRARIES
        ${CET_LIBRARIES}
        )
    endif()
  endif()

  if(NOT CET_NO_AUTO)
    # If GLOBAL is not set, prepend ${CETMODULES_CURRENT_PROJECT_NAME}: to the
    # target name
    if(CET_SCOPED)
      set(TEST_TARGET_NAME "${CETMODULES_CURRENT_PROJECT_NAME}:${CET_TARGET}")
    else()
      set(TEST_TARGET_NAME "${CET_TARGET}")
    endif()

    # For which configurations should this test (set) be valid? Print configured
    # permuted arguments.
    _cet_print_pargs("${parg_labels}")

    # Set up to handle a per-test work directory for parallel testing.
    if(IS_ABSOLUTE "${CET_TEST_WORKDIR}")
      cet_package_path(source_path_subdir PATH "${CET_TEST_WORKDIR}" SOURCE)
      if(source_path_subdir) # Be careful in source tree.
        if(NOT IS_DIRECTORY "${CET_TEST_WORKDIR}")
          message(
            SEND_ERROR
              "Refusing to create working directory ${CET_TEST_WORKDIR} in source tree"
            )
        endif()
        set(CET_DIRTY_WORKDIR TRUE)
      endif()
    else()
      if(NOT CET_TEST_WORKDIR)
        set(CET_TEST_WORKDIR "${CET_TARGET}.d")
      endif()
      get_filename_component(
        CET_TEST_WORKDIR "${CET_TEST_WORKDIR}" ABSOLUTE BASE_DIR
        "${CMAKE_CURRENT_BINARY_DIR}"
        )
    endif()
    file(MAKE_DIRECTORY "${CET_TEST_WORKDIR}")
    cet_passthrough(FLAG IN_PLACE KEYWORD --dirty-workdir CET_DIRTY_WORKDIR)

    # Determine the skip return code
    list(FIND CET_TEST_PROPERTIES SKIP_RETURN_CODE skip_return_code_idx)
    if(skip_return_code_idx GREATER -1)
      math(EXPR skip_return_code_idx "${skip_return_code_idx} + 1")
      list(GET CET_TEST_PROPERTIES ${skip_return_code_idx} skip_return_code)
    else()
      set(skip_return_code 247)
      list(APPEND CET_TEST_PROPERTIES SKIP_RETURN_CODE ${skip_return_code})
    endif()

    # Deal with test parameters based on mode.
    if(CET_TEST_EXEC)
      if(NOT CET_HANDBUILT)
        message(FATAL_ERROR "cet_test: target ${CET_TARGET} cannot specify "
                            "TEST_EXEC without HANDBUILT"
                )
      endif()
    elseif(CET_COMPILE_ONLY)
      if(NTESTS GREATER 1)
        message(
          FATAL_ERROR
            "cet_test: COMPILE_ONLY is incompatible with parameterized tests (NTESTS = ${NTESTS})"
          )
      endif()
      get_property(
        CETMODULES_COMPILE_ONLY_TEST_ENABLED_LANGUAGES GLOBAL
        PROPERTY ENABLED_LANGUAGES
        )
      set(CETMODULES_COMPILE_ONLY_TEST_COMPILE_COMMANDS
          "\
include(CetMake)
cet_make_exec(NAME ${CET_TARGET}
  ${exec_extra_args}
  ${CET_USE_BOOST_UNIT} ${CET_USE_CATCH2_MAIN}
  SOURCE ${CET_SOURCE} LIBRARIES ${CET_LIBRARIES}
)\
"
          )
      cet_localize_pv(cetmodules ETC_DIR)
      configure_file(
        ${cetmodules_ETC_DIR}/CompileOnlyTest.cmake.in
        ${CET_TARGET}-src/CMakeLists.txt @ONLY
        )
      configure_file(
        ${cetmodules_ETC_DIR}/CompileOnlyTest.in
        ${CET_TARGET}-src/${CET_TARGET}
        @ONLY
        FILE_PERMISSIONS
        OWNER_READ
        OWNER_WRITE
        OWNER_EXECUTE
        GROUP_READ
        GROUP_EXECUTE
        WORLD_READ
        WORLD_EXECUTE
        )
      list(APPEND CET_DATAFILES ${CET_SOURCE}
           ${CMAKE_CURRENT_BINARY_DIR}/${CET_TARGET}-src/CMakeLists.txt
           )
      set(CET_TEST_EXEC
          ${CMAKE_CURRENT_BINARY_DIR}/${CET_TARGET}-src/${CET_TARGET}
          )
      set(CET_TEST_ARGS --config $<CONFIG>)
    else()
      set(CET_TEST_EXEC ${CET_TARGET})
    endif()

    # Deal with specified data files.
    if(DEFINED CET_DATAFILES)
      list(REMOVE_DUPLICATES CET_DATAFILES)
      set(datafiles_tmp)
      foreach(df IN LISTS CET_DATAFILES)
        get_filename_component(dfd ${df} DIRECTORY)
        if(dfd)
          list(APPEND datafiles_tmp ${df})
        else(dfd)
          list(APPEND datafiles_tmp ${CMAKE_CURRENT_SOURCE_DIR}/${df})
        endif(dfd)
      endforeach()
      set(CET_DATAFILES ${datafiles_tmp})
    endif(DEFINED CET_DATAFILES)

    # Handle CMake test labels.
    if(NOT (CET_OPTIONAL_GROUPS OR CET_NO_OPTIONAL_GROUPS))
      set(CET_OPTIONAL_GROUPS DEFAULT RELEASE)
    endif()
    if(NTESTS GREATER 1)
      list(APPEND CET_OPTIONAL_GROUPS ${TEST_TARGET_NAME})
    endif()
    if(CET_COMPILE_ONLY)
      list(APPEND CET_OPTIONAL_GROUPS COMPILE_ONLY)
    endif()
    _update_defined_test_groups(${CET_OPTIONAL_GROUPS})

    if(CET_REF)
      if("PASS_REGULAR_EXPRESSION" IN_LIST CET_TEST_PROPERTIES
         OR "FAIL_REGULAR_EXPRESSION" IN_LIST CET_TEST_PROPERTIES
         )
        message(
          FATAL_ERROR
            "Cannot specify REF option for test ${CET_TARGET} in conjunction with (PASS|FAIL)_REGULAR_EXPESSION."
          )
      endif()
      list(POP_FRONT CET_REF OUTPUT_REF ERROR_REF)
      set(CET_REF ${OUTPUT_REF} ${ERROR_REF})
      if(ERROR_REF)
        set(DEFINE_ERROR_REF "-DTEST_REF_ERR=${ERROR_REF}")
        set(DEFINE_TEST_ERR "-DTEST_ERR=${CET_TARGET}.err")
      endif()
      if(CET_OUTPUT_FILTER)
        string(JOIN " " CET_OUTPUT_FILTERS ${CET_OUTPUT_FILTER}
               ${CET_OUTPUT_FILTER_ARGS}
               )
      endif()
      if(CET_OUTPUT_FILTERS)
        foreach(filter IN LISTS CET_OUTPUT_FILTERS)
          separate_arguments(args NATIVE_COMMAND "${filter}")
          set(filter)
          foreach(arg IN LISTS args)
            _cet_exec_location(arg "${arg}")
            list(APPEND filter "${arg}")
          endforeach()
          list(APPEND DEFINE_OUTPUT_FILTERS "${filter}")
        endforeach()
        string(REPLACE ";" "\\;" DEFINE_OUTPUT_FILTERS
                       "${DEFINE_OUTPUT_FILTERS}"
               )
        string(PREPEND DEFINE_OUTPUT_FILTERS "-DOUTPUT_FILTERS=")
      endif()
      _cet_add_ref_test(${CET_TEST_ARGS})
    else(CET_REF)
      _cet_add_test(${CET_TEST_ARGS})
    endif(CET_REF)
    set_tests_properties(
      ${ALL_TEST_TARGETS} PROPERTIES LABELS "${CET_OPTIONAL_GROUPS}"
      )
    if(CET_TEST_PROPERTIES)
      set_tests_properties(
        ${ALL_TEST_TARGETS} PROPERTIES ${CET_TEST_PROPERTIES}
        )
    endif()
    if(CET_REQUIRED_TESTS)
      project_variable(
        TEST_DEPS_AS_FIXTURES
        TYPE
        BOOL
        DOCSTRING
        "\
If TRUE, test dependencies identified via cet_test(... REQUIRED_TESTS) are treated as REQUIRED_FIXTURES and added to the test selection if missing; otherwise REQUIRED_TESTS only specifies execution order if dependencies are selected\
"
        FALSE
        )
      if(${CETMODULES_CURRENT_PROJECT_NAME}_TEST_DEPS_AS_FIXTURES)
        list(APPEND CET_REQUIRED_FIXTURES ${CET_REQUIRED_TESTS})
      else()
        set_property(
          TEST ${ALL_TEST_TARGETS}
          APPEND
          PROPERTY DEPENDS "${CET_REQUIRED_TESTS}"
          )
      endif()
    endif()
    if(CET_REQUIRED_FIXTURES)
      foreach(test IN LISTS CET_REQUIRED_FIXTURES)
        if(NOT TEST ${test})
          message(
            FATAL_ERROR
              "\
test ${test} must be defined already to be specified as a fixture for ${CET_TARGET}\
"
            )
        endif()
        get_property(
          fixture_name
          TEST ${test}
          PROPERTY FIXTURES_SETUP
          )
        if(NOT fixture_name)
          set(fixture_name "${test}")
          set_property(TEST ${test} PROPERTY FIXTURES_SETUP "${fixture_name}")
        endif()
        set_property(
          TEST ${ALL_TEST_TARGETS}
          APPEND
          PROPERTY FIXTURES_REQUIRED "${fixture_name}"
          )
      endforeach()
    endif()
    if(CETB_SANITIZER_PRELOADS)
      set_property(
        TEST ${ALL_TEST_TARGETS}
        APPEND
        PROPERTY ENVIRONMENT
                 "LD_PRELOAD=$ENV{LD_PRELOAD} ${CETB_SANITIZER_PRELOADS}"
        )
    endif()
    foreach(san_env IN ITEMS ASAN_OPTIONS MSAN_OPTIONS LSAN_OPTIONS
                             TSAN_OPTIONS UBSAN_OPTIONS
            )
      if(CETP_${san_env})
        set_property(
          TEST ${ALL_TEST_TARGETS}
          APPEND
          PROPERTY ENVIRONMENT "${san_env}=${CETP_${san_env}}"
          )
      elseif(DEFINED ENV{${san_env}})
        set_property(
          TEST ${ALL_TEST_TARGETS}
          APPEND
          PROPERTY ENVIRONMENT "${san_env}=$ENV{${san_env}}"
          )
      endif()
    endforeach()
    foreach(target IN LISTS ALL_TEST_TARGETS)
      if(CET_TEST_ENV)
        get_test_property(${target} ENVIRONMENT CET_TEST_ENV_TMP)
        if(CET_TEST_ENV_TMP)
          set_tests_properties(
            ${target} PROPERTIES ENVIRONMENT
                                 "${CET_TEST_ENV};${CET_TEST_ENV_TMP}"
            )
        else()
          set_tests_properties(
            ${target} PROPERTIES ENVIRONMENT "${CET_TEST_ENV}"
            )
        endif()
      endif()
      if(CET_TEST_ENV_MODIFICATION)
        get_test_property(${target} ENVIRONMENT_MODIFICATION CET_TEST_ENV_TMP)
        if(CET_TEST_ENV_TMP)
          set_tests_properties(
            ${target}
            PROPERTIES ENVIRONMENT_MODIFICATION
                       "${CET_TEST_ENV_MODIFICATION};${CET_TEST_ENV_TMP}"
            )
        else()
          set_tests_properties(
            ${target} PROPERTIES ENVIRONMENT_MODIFICATION
                                 "${CET_TEST_ENV_MODIFICATION}"
            )
        endif()
      endif()
      if(CET_COMPILE_ONLY)
        cet_transitive_paths(BINARY_DIR IN_TREE)
        list(PREPEND TRANSITIVE_PATHS_WITH_BINARY_DIR "${cetmodules_DIR}")
        list(JOIN TRANSITIVE_PATHS_WITH_BINARY_DIR ":" DIRS_FOR_PREFIX_PATH)
        set(TEST_CMAKE_PREFIX_PATH
            "CMAKE_PREFIX_PATH=path_list_prepend:${DIRS_FOR_PREFIX_PATH}"
            )
        get_test_property(${target} ENVIRONMENT_MODIFICATION CET_TEST_ENV_TMP)
        if(CET_TEST_ENV_TMP)
          set_tests_properties(
            ${target}
            PROPERTIES ENVIRONMENT_MODIFICATION
                       "${TEST_CMAKE_PREFIX_PATH};${CET_TEST_ENV_TMP}"
            )
        else()
          set_tests_properties(
            ${target} PROPERTIES ENVIRONMENT_MODIFICATION
                                 "${TEST_CMAKE_PREFIX_PATH}"
            )
        endif()
        cmake_language(
          EVAL
          CODE
          "cmake_language(DEFER CALL
             cmake_language EVAL CODE
             \"get_property(will_fail TEST ${target} PROPERTY WILL_FAIL)
             if (will_fail)
               set(failit PASS)
             else()
               set(failit FAIL)
             endif()
             set_property(TEST ${target} APPEND PROPERTY \\\${failit}_REGULAR_EXPRESSION [=[<<ERROR: build system failure for COMPILE_ONLY test>>
]=])\")"
          )
      endif()
      if(CET_REF)
        get_test_property(${target} REQUIRED_FILES REQUIRED_FILES_TMP)
        if(REQUIRED_FILES_TMP)
          set_tests_properties(
            ${target} PROPERTIES REQUIRED_FILES
                                 "${REQUIRED_FILES_TMP};${CET_REF}"
            )
        else()
          set_tests_properties(${target} PROPERTIES REQUIRED_FILES "${CET_REF}")
        endif()
      else(CET_REF)
        if(CET_OUTPUT_FILTER OR CET_OUTPUT_FILTER_ARGS)
          message(
            FATAL_ERROR
              "OUTPUT_FILTER and OUTPUT_FILTER_ARGS are not accepted if REF is not specified."
            )
        endif()
      endif()
    endforeach()
  else(NOT CET_NO_AUTO)
    if(CET_CONFIGURATIONS
       OR CET_DATAFILES
       OR CET_DIRTY_WORKDIR
       OR CET_NO_OPTIONAL_GROUPS
       OR CET_OPTIONAL_GROUPS
       OR CET_OUTPUT_FILTER
       OR CET_OUTPUT_FILTERS
       OR CET_OUTPUT_FILTER_ARGS
       OR CET_REF
       OR CET_REQUIRED_FILES
       OR CET_SCOPED
       OR CET_TEST_ARGS
       OR CET_TEST_PROPERTIES
       OR CET_TEST_WORKDIR
       OR NPARG_LABELS
       )
      message(
        FATAL_ERROR
          "The following arguments are not meaningful in the presence of NO_AUTO:
CONFIGURATIONS DATAFILES DIRTY_WORKDIR NO_OPTIONAL_GROUPS_OPTIONAL_GROUPS OUTPUT_FILTER OUTPUT_FILTERS OUTPUT_FILTER_ARGS PARG_<label> REF REQUIRED_FILES SCOPED TEST_ARGS TEST_PROPERTIES TEST_WORKDIR"
        )
    endif()
  endif(NOT CET_NO_AUTO)

  if(CET_INSTALL_BIN AND CET_HANDBUILT)
    message(WARNING "INSTALL_BIN option ignored for HANDBUILT tests.")
  endif()
  if(CET_INSTALL_EXAMPLE)
    # Install to examples directory of product.
    install(FILES "${CET_SOURCE}" ${CET_DATAFILES} DESTINATION example)
  endif()
  if(CET_INSTALL_SOURCE)
    cet_package_path(CURRENT_SUBDIR)
    cet_regex_escape(
      "${${CETMODULES_CURRENT_PROJECT_NAME}_TEST_DIR}" e_test_dir
      )
    string(REGEX REPLACE "^(test|${e_test_dir})(/|$)" "" CURRENT_SUBDIR
                         "${CURRENT_SUBDIR}"
           )
    install(
      FILES "${CET_SOURCE}"
      DESTINATION
        "${${CETMODULES_CURRENT_PROJECT_NAME}_TEST_DIR}/${CURRENT_SUBDIR}"
      )
  endif()
endfunction(cet_test)

#[================================================================[.rst:
.. command:: cet_test_assertion

   Look for the specified assertion failure in test output for the
   specified targets.

   .. code-block:: cmake

      cet_test_assertion(<condition> <target> ...)

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<condition>``
     .. rst-class:: text-start

     Look for the string ``Assertion failed: (<condition>),`` (Darwin)
     or ``Assertion `<condition>' failed.`` (Linux).

   ``<target>``
     One or more targets to be subject to this test.

   .. seealso::

     :prop_test:`PASS_REGULAR_EXPRESSION
     <cmake-ref-current:prop_test:PASS_REGULAR_EXPRESSION>`.

#]================================================================]

function(cet_test_assertion CONDITION FIRST_TARGET)
  if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set_tests_properties(
      ${FIRST_TARGET} ${ARGN}
      PROPERTIES PASS_REGULAR_EXPRESSION
                 "Assertion failed: \\(${CONDITION}\\), "
      )
  else()
    set_tests_properties(
      ${FIRST_TARGET} ${ARGN} PROPERTIES PASS_REGULAR_EXPRESSION
                                         "Assertion `${CONDITION}' failed\\."
      )
  endif()
endfunction()

#[================================================================[.rst:
.. command:: cet_test_env

   Add environment variables to the test environment for tests defined
   in and below the current directory.

   .. seealso:: :variable:`CET_TEST_ENV`.

   .. code-block:: cmake

      cet_test_env([CLEAR] <var>=<val> ...)

   Options
   ^^^^^^^

   ``CLEAR``
     Clear the test environment in the current directory scope prior to
     setting ``<var>=<val>``, including any environment modifications.

     .. seealso:: :command:`cet_test_env_prepend`,
                  :variable:`CET_TEST_ENV_MODIFICATION`.

   Details
   ^^^^^^^

   If the current test environment already has an entry for ``<var>``,
   it will be superseded by ``<val>``.

#]================================================================]

function(cet_test_env)
  cmake_parse_arguments(PARSE_ARGV 0 CET_TEST "CLEAR" "" "")
  if(CET_TEST_CLEAR)
    set(CET_TEST_ENV)
  endif()
  # Remove existing settings for environment variables.
  list(TRANSFORM CET_TEST_UNPARSED_ARGUMENTS REPLACE "=.*" "" OUTPUT_VARIABLE
                                                              env_vars
       )
  foreach(ev env_vars)
    cet_regex_escape("${ev}" ev)
    list(FILTER CET_TEST_ENV EXCLUDE REGEX "^${ev}=")
  endforeach()
  list(APPEND CET_TEST_ENV "${CET_TEST_UNPARSED_ARGUMENTS}")
  set(CET_TEST_ENV
      "${CET_TEST_ENV}"
      PARENT_SCOPE
      )
endfunction()

#[================================================================[.rst:
.. command:: cet_test_env_mod

   Add environment modifications to the test environment for tests
   defined in and below the current directory.

   .. seealso:: :variable:`CET_TEST_ENV_MODIFICATION`,
                :prop_test:`cmake-ref-current:prop_test:ENVIRONMENT_MODIFICATION`.

   .. code-block:: cmake

      cet_test_env_mod(<var> <op> [CLEAR] [REMOVE_DUPLICATES] <dir> ...)

   Options
   ^^^^^^^

   ``CLEAR``
     Clear any existing test environment modifications in the current
     directory scope.

   ``REMOVE_DUPLICATES``
     Remove any duplicate ``<dir> ...``; for a given duplicate, only the
     first entry will be kept.

#]================================================================]

function(cet_test_env_mod VAR OP)
  cmake_parse_arguments(PARSE_ARGV 2 CET_TEM "CLEAR;REMOVE_DUPLICATES" "" "")
  if(CET_TEM_CLEAR)
    set(CET_TEST_ENV_MODIFICATION)
  endif()
  if(CET_TEM_REMOVE_DUPLICATES)
    list(REMOVE_DUPLICATES CET_TEM_UNPARSED_ARGUMENTS)
  endif()
  string(REPLACE ";" ":" test_env_mod "${CET_TEM_UNPARSED_ARGUMENTS}")
  list(APPEND CET_TEST_ENV_MODIFICATION "${VAR}=${OP}:${test_env_mod}")
  set(CET_TEST_ENV_MODIFICATION
      "${CET_TEST_ENV_MODIFICATION}"
      PARENT_SCOPE
      )
endfunction()

#[================================================================[.rst:
.. command:: cet_test_env_prepend

   Prepend to path-like environment variables in the test environment
   for tests defined in and below the current directory.

   .. code-block:: cmake

      cet_test_env_prepend(<var> ...)

   Functionally identical to :command:`cet_test_env_mod(<var>
   path_list_prepend ...)  <cet_test_env_mod>`

#]================================================================]

function(cet_test_env_prepend CET_ENV_VAR)
  cet_test_env_mod(${CET_ENV_VAR} path_list_prepend ${ARGN})
  set(CET_TEST_ENV_MODIFICATION
      "${CET_TEST_ENV_MODIFICATION}"
      PARENT_SCOPE
      )
endfunction()

function(_cet_add_ref_test)
  if(${NTESTS} EQUAL 1)
    _cet_add_ref_test_detail(${TEST_TARGET_NAME} ${CET_TEST_WORKDIR} ${ARGN})
    list(APPEND ALL_TEST_TARGETS ${TEST_TARGET_NAME})
    file(MAKE_DIRECTORY "${CET_TEST_WORKDIR}")
    set_tests_properties(
      ${TEST_TARGET_NAME} PROPERTIES WORKING_DIRECTORY ${CET_TEST_WORKDIR}
      )
    cet_copy(${CET_DATAFILES} DESTINATION ${CET_TEST_WORKDIR} WORKING_DIRECTORY
             ${CMAKE_CURRENT_SOURCE_DIR}
             )
  else()
    math(EXPR tidmax "${NTESTS} - 1")
    string(LENGTH "${tidmax}" nd)
    foreach(tid RANGE ${tidmax})
      execute_process(
        COMMAND printf "_%0${nd}d" ${tid}
        OUTPUT_VARIABLE tnum
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
      set(tname "${TEST_TARGET_NAME}${tnum}")
      string(REGEX REPLACE [[\.d$]] "${tnum}.d" test_workdir
                           "${CET_TEST_WORKDIR}"
             )
      _cet_add_ref_test_detail(${tname} ${test_workdir} ${ARGN})
      list(APPEND ALL_TEST_TARGETS ${tname})
      file(MAKE_DIRECTORY "${test_workdir}")
      set_tests_properties(
        ${tname} PROPERTIES WORKING_DIRECTORY ${test_workdir}
        )
      if(CET_DATAFILES)
        cet_copy(${CET_DATAFILES} DESTINATION ${test_workdir} WORKING_DIRECTORY
                 ${CMAKE_CURRENT_SOURCE_DIR}
                 )
      endif()
    endforeach()
  endif()
  set(ALL_TEST_TARGETS
      ${ALL_TEST_TARGETS}
      PARENT_SCOPE
      )
endfunction()

function(_cet_add_ref_test_detail TNAME TEST_WORKDIR)
  _cet_test_pargs(tmp_args ${ARGN})
  separate_arguments(test_args UNIX_COMMAND "${tmp_args}")
  cet_localize_pv(cetmodules LIBEXEC_DIR)
  _cet_exec_location(TEXEC ${CET_TEST_EXEC})
  add_test(
    NAME "${TNAME}"
    CONFIGURATIONS ${CET_CONFIGURATIONS}
    COMMAND
      ${CET_TEST_WRAPPER} --wd ${TEST_WORKDIR} --remove-on-failure
      "${CET_REMOVE_ON_FAILURE}" --required-files "${CET_REQUIRED_FILES}"
      --datafiles "${CET_DATAFILES}" ${CET_DIRTY_WORKDIR} --skip-return-code
      ${skip_return_code} ${CMAKE_COMMAND} -DTEST_EXEC=${TEXEC}
      -DTEST_ARGS=${test_args} -DTEST_REF=${OUTPUT_REF} ${DEFINE_ERROR_REF}
      ${DEFINE_TEST_ERR} -DTEST_OUT=${CET_TARGET}.out ${DEFINE_OUTPUT_FILTER}
      ${DEFINE_OUTPUT_FILTER_ARGS} ${DEFINE_OUTPUT_FILTERS}
      -Dcetmodules_LIBEXEC_DIR=${cetmodules_LIBEXEC_DIR} -P
      ${CET_RUNANDCOMPARE}
    )
endfunction()

function(_cet_add_test)
  _cet_exec_location(TEXEC ${CET_TEST_EXEC})
  if(${NTESTS} EQUAL 1)
    _cet_add_test_detail(
      ${TEST_TARGET_NAME} ${TEXEC} ${CET_TEST_WORKDIR} ${ARGN}
      )
    list(APPEND ALL_TEST_TARGETS ${TEST_TARGET_NAME})
    file(MAKE_DIRECTORY "${CET_TEST_WORKDIR}")
    set_tests_properties(
      ${TEST_TARGET_NAME} PROPERTIES WORKING_DIRECTORY ${CET_TEST_WORKDIR}
      )
    cet_copy(${CET_DATAFILES} DESTINATION ${CET_TEST_WORKDIR} WORKING_DIRECTORY
             ${CMAKE_CURRENT_SOURCE_DIR}
             )
  else()
    math(EXPR tidmax "${NTESTS} - 1")
    string(LENGTH "${tidmax}" nd)
    foreach(tid RANGE ${tidmax})
      execute_process(
        COMMAND printf "_%0${nd}d" ${tid}
        OUTPUT_VARIABLE tnum
        OUTPUT_STRIP_TRAILING_WHITESPACE
        )
      set(tname "${TEST_TARGET_NAME}${tnum}")
      string(REGEX REPLACE [[\.d$]] "${tnum}.d" test_workdir
                           "${CET_TEST_WORKDIR}"
             )
      _cet_add_test_detail(${tname} ${TEXEC} ${test_workdir} ${ARGN})
      list(APPEND ALL_TEST_TARGETS ${tname})
      file(MAKE_DIRECTORY "${test_workdir}")
      set_tests_properties(
        ${tname} PROPERTIES WORKING_DIRECTORY ${test_workdir}
        )
      cet_copy(${CET_DATAFILES} DESTINATION ${test_workdir} WORKING_DIRECTORY
               ${CMAKE_CURRENT_SOURCE_DIR}
               )
    endforeach()
  endif()
  set(ALL_TEST_TARGETS
      ${ALL_TEST_TARGETS}
      PARENT_SCOPE
      )
endfunction()

function(_cet_add_test_detail TNAME TEXEC TEST_WORKDIR)
  _cet_test_pargs(test_args ${ARGN})
  add_test(
    NAME "${TNAME}"
    CONFIGURATIONS ${CET_CONFIGURATIONS}
    COMMAND
      ${CET_TEST_WRAPPER} --wd ${TEST_WORKDIR} --remove-on-failure
      "${CET_REMOVE_ON_FAILURE}" --required-files "${CET_REQUIRED_FILES}"
      --datafiles "${CET_DATAFILES}" ${CET_DIRTY_WORKDIR} --skip-return-code
      ${skip_return_code} ${TEXEC} ${test_args}
    )
  _cet_add_test_properties(${TNAME} ${TEXEC})
endfunction()

# FIXME: Needs work!
function(_cet_add_test_properties TEST_NAME TEST_EXEC)
  if(NOT TARGET ${TEST_EXEC}) # Not interested.
    return()
  endif()
  set_property(
    TEST ${TEST_NAME}
    APPEND
    PROPERTY KEYWORDS CET $<TARGET_PROPERTY:${TEST_EXEC}>
    )
endfunction()

function(_cet_exec_location LOC_VAR)
  list(POP_FRONT ARGN EXEC)
  string(TOUPPER "${EXEC}" EXEC_UC)
  if(DEFINED ${EXEC_UC} AND NOT "${EXEC}" STREQUAL "${${EXEC_UC}}")
    set(EXEC "${${EXEC_UC}}")
    if("${EXEC}" STREQUAL "") # Empty.
      set(${LOC_VAR}
          ""
          PARENT_SCOPE
          )
      return()
    endif()
    _cet_exec_location(EXEC "${EXEC}")
  endif()
  if(TARGET "${EXEC}")
    # FIXME: could load all this up as a generator expression if we cared enough
    # to deal with targets not being defined yet.
    get_property(
      target_type
      TARGET ${EXEC}
      PROPERTY TYPE
      )
    if(target_type STREQUAL "EXECUTABLE")
      set(EXEC "$<TARGET_FILE:${EXEC}>")
    else()
      get_property(
        imported
        TARGET ${EXEC}
        PROPERTY IMPORTED
        )
      if(imported)
        set(EXEC "$<TARGET_PROPERTY:${EXEC},IMPORTED_LOCATION>")
      else()
        get_property(
          exec_location
          TARGET ${EXEC}
          PROPERTY CET_EXEC_LOCATION
          )
        if(exec_location)
          set(EXEC "${exec_location}")
        endif()
      endif()
    endif()
  endif()
  set(${LOC_VAR}
      "${EXEC}"
      PARENT_SCOPE
      )
endfunction()

function(_cet_print_pargs)
  string(TOUPPER "${CMAKE_BUILD_TYPE}" BTYPE_UC)
  if(NOT BTYPE_UC STREQUAL "DEBUG")
    return()
  endif()
  list(LENGTH ARGN nlabels)
  if(NOT nlabels)
    return()
  endif()
  message(
    STATUS
      "Test ${TEST_TARGET_NAME}: found ${nlabels} labels for permuted test arguments"
    )
  foreach(label IN LISTS ARGN)
    message(
      STATUS
        "  Label: ${label}, arg: ${${label}_arg}, # vals: ${${label}_length}, vals: ${CETP_PARG_${label}}"
      )
  endforeach()
  message(STATUS "  Calculated ${NTESTS} tests")
endfunction()

function(_cet_process_pargs NTEST_VAR)
  set(NTESTS 1)
  foreach(label IN LISTS ARGN)
    list(LENGTH CETP_PARG_${label} ${label}_length)
    math(EXPR ${label}_length "${${label}_length} - 1")
    if(NOT ${label}_length)
      message(
        FATAL_ERROR
          "For test ${TEST_TARGET_NAME}: Permuted options are not yet supported."
        )
    endif()
    if(${label}_length GREATER NTESTS)
      set(NTESTS ${${label}_length})
    endif()
    list(GET CETP_PARG_${label} 0 ${label}_arg)
    set(${label}_arg
        ${${label}_arg}
        PARENT_SCOPE
        )
    list(REMOVE_AT CETP_PARG_${label} 0)
    set(CETP_PARG_${label}
        ${CETP_PARG_${label}}
        PARENT_SCOPE
        )
    set(${label}_length
        ${${label}_length}
        PARENT_SCOPE
        )
  endforeach()
  foreach(label IN LISTS ARGN)
    if(${label}_length LESS NTESTS)
      # Need to pad
      math(EXPR nextra "${NTESTS} - ${${label}_length}")
      set(nind 0)
      while(nextra)
        math(EXPR lind "${nind} % ${${label}_length}")
        list(GET CETP_PARG_${label} ${lind} item)
        list(APPEND CETP_PARG_${label} ${item})
        math(EXPR nextra "${nextra} - 1")
        math(EXPR nind "${nind} + 1")
      endwhile()
      set(CETP_PARG_${label}
          ${CETP_PARG_${label}}
          PARENT_SCOPE
          )
    endif()
  endforeach()
  set(${NTEST_VAR}
      ${NTESTS}
      PARENT_SCOPE
      )
endfunction()

function(_cet_test_pargs VAR)
  foreach(label IN LISTS parg_labels)
    list(GET CETP_PARG_${label} ${tid} arg)
    if(${label}_arg MATCHES [[=$]])
      list(APPEND test_args "${${label}_arg}${arg}")
    else()
      list(APPEND test_args "${${label}_arg}" "${arg}")
    endif()
  endforeach()
  set(${VAR}
      ${test_args} ${ARGN}
      PARENT_SCOPE
      )
endfunction()

function(_update_defined_test_groups)
  set(TMP_LIST ${CET_DEFINED_TEST_GROUPS} ${ARGN})
  list(REMOVE_DUPLICATES TMP_LIST)
  set(CET_DEFINED_TEST_GROUPS
      ${TMP_LIST}
      CACHE STRING "List of defined test groups." FORCE
      )
endfunction()
