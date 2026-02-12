#[================================================================[.rst:
BuildDictionary
---------------

.. admonition:: ROOT
   :class: admonition-app

   Module defining the function :command:`build_dictionary` to generate
   a ROOT dictionary from a selection XML (:file:`classes_def.xml`).

.. seealso::

   `ROOT Home Page <https://root.cern.ch>`_

   :module:`CetRootCint`
     Building a ROOT dictionary from a :file:`Linkdef.h` file.

#]================================================================]

include_guard()

cmake_minimum_required(VERSION 3.19...4.1 FATAL_ERROR)

include(CetCMakeUtils)
include(CetCopy)
include(CetMakeLibrary)
include(CetPackagePath)
include(CetProcessLiblist)
include(CheckClassVersion)

set(_cet_build_dictionary_flags
    NOP
    NO_CHECK_CLASS_VERSION
    NO_EXPORT
    NO_INSTALL
    NO_LIBRARY
    NO_RECURSIVE
    RECURSIVE
    USE_PRODUCT_NAME
    USE_PROJECT_NAME
    VERBOSE
    )

set(_cet_build_dictionary_one_arg_options
    CLASSES_DEF_XML
    CLASSES_H
    DICT_NAME_VAR
    EXPORT_SET
    GENERATED_SOURCE_FILENAME
    LIB_TARGET
    LIB_TARGET_VAR
    MODULEMAP_INSTALL_DIR
    VERBOSITY
    )

set(_cet_build_dictionary_list_options
    CCV_ENVIRONMENT COMPILE_FLAGS DICTIONARY_LIBRARIES REQUIRED_DICTIONARIES
    SOURCE
    )

#[================================================================[.rst:
.. command:: build_dictionary

   Generate and build a ROOT dictionary module from either:

   * a selection XML file (:file:`classes_def.xml`), optionally checking
   versions and checksums for selected classes; *or*

   * a :file:`LinkDef.h` or equivalent (*cf* :command:`cet_rootcint`).

   .. code-block:: cmake

      build_dictionary([<name>] [<options>])

   Options
   ^^^^^^^

   ``CCV_ENVIRONMENT <var>=<val>...``
     List of environment settings to pass to
     :manual:`checkClassVersion(1)`.

   ``CLASSES_DEF_XML <filepath>``
     .. rst-class:: text-start

     The name and location of the selection XML file to be used
     (default: :variable:`${CMAKE_CURRENT_SOURCE_DIR}
     <cmake-ref-current:variable:CMAKE_CURRENT_SOURCE_DIR>`:file:`/classes_def.xml`).

     .. deprecated:: 3.23.00

        Use :ref:`SOURCE \<filepath> <build_dictionary-SOURCE>` instead.

   ``CLASSES_H <filepath>``
     .. rst-class:: text-start

     The name and location of the top-level C++ header file to be read
     (default: :variable:`${CMAKE_CURRENT_SOURCE_DIR}
     <cmake-ref-current:variable:CMAKE_CURRENT_SOURCE_DIR>`:file:`/classes.h`).

     .. deprecated:: 3.23.00

        use :ref:`SOURCE \<filepath> <build_dictionary-SOURCE>` instead.

   ``COMPILE_FLAGS <flag>...``
     Extra compilation options.

   ``DICTIONARY_LIBRARIES <library-dependency>...``
     Libraries to which to link the dictionary plugin.

   ``DICT_NAME_VAR <var>``
     Variable in which to store the dictionary's name (useful when
     generated).

   ``EXPORT_SET <export-name>``
     Add the library to the ``<export-name>`` export set.

   ``GENERATED_SOURCE_FILENAME <filename>``
     Use ``<filename>`` for the generated source (default
     ``<lib-target>.cpp``).

   ``LIB_TARGET <target>``
     The name of the library target intended to contain the object code
     resulting from the compilation of the generated dictionary
     source. Defaults to ``<generated-dictionary-name>_dict`` if not
     specified.

     .. seealso:: :ref:`\<name> <build_dictionary-name>`.

   ``LIB_TARGET_VAR <var>``
     Variable in which to store the name of the library target to which
     the generated dictionary source will be added (useful when
     generated).

   ``MODULEMAP_INSTALL_DIR``
     Installation location for the (possibly composite)
     :file:`module.modulemap` file if present (default
     ``${CETMODULES_CURRENT_PROJECT_NAME}_INCLUDE_DIR``).

   ``NOP``
     Option / argument disambiguator; no other function.

   .. _build_dictionary-NO_CHECK_CLASS_VERSION:

   ``NO_CHECK_CLASS_VERSION``
     Do not run :manual:`checkClassVersion(1)` to verify class checksums
     and version numbers.

   ``NO_EXPORT``
     Do not export the generated plugin.

   ``NO_INSTALL``
     Do not install the generated plugin.

   ``NO_LIBRARY``
     .. rst-class:: text-start

     Generate the C++ code, but do not compile it into a shared library
     (implies :ref:`NO_CHECK_CLASS_VERSION
     <build_dictionary-NO_CHECK_CLASS_VERSION>`).

   ``[NO_]RECURSIVE``
     Specify whether :manual:`checkClassVersion(1)` should check for the
     presence and validity of class dictionaries recursively (default
     determined by :command:`check_class_version`).

   ``REQUIRED_DICTIONARIES <dictionary-dependency>...``
     .. deprecated:: 3.23.00
        Ignored.

   .. _build_dictionary-SOURCE:

   ``SOURCE <filepath> ...``
     Pass ``<filepath>`` to :program:`rootcint`.

     ``<filepath>`` should be:

     a. valid as an argument to a pre-processor ``#include`` directive, or

     a. a path (absolute, or relative to
        :variable:`${CMAKE_CURRENT_SOURCE_DIR}
        <cmake-ref-current:variable:CMAKE_CURRENT_SOURCE_DIR>`) to either
        an XML selection file (e.g. :file:`classes_def.xml`), a
        :file:`classes.h` file, or a :file:`LinkDef.h` file.

   ``USE_PRODUCT_NAME``
     .. deprecated:: 2.0 use ``USE_PROJECT_NAME`` instead.

   .. _build_dictionary-USE_PROJECT_NAME:

   ``USE_PROJECT_NAME``
     .. versionadded:: 3.23.00

     The project name will be prepended to the plugin library name,
     separated by ``_``

   ``VERBOSE``
     Increase the verbosity of the dictionary generation command;
     equivalent to ``VERBOSITY 4``.

   ``VERBOSITY <n>``
     Set the verbosity of the dictionary generation (default 2).

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   .. _build_dictionary-name:

   ``<name>``
     The desired name of the dictionary (will be modified by
     :ref:`USE_PROJECT_NAME <build_dictionary-USE_PROJECT_NAME>` if
     present). If not specified, it will be generated from the current
     source directory's path relative to the project's top level
     directory via :command:`cet_package_path`.

   Notes
   ^^^^^

   .. seealso:: :command:`cet_make_library`, :command:`check_class_version`

#]================================================================]

function(build_dictionary)
  cmake_parse_arguments(
    PARSE_ARGV 0 BD "${_cet_build_dictionary_flags}"
    "${_cet_build_dictionary_one_arg_options}"
    "${_cet_build_dictionary_list_options}"
    )
  if(BD_REQUIRED_DICTIONARIES)
    warn_deprecated("REQUIRED_DICTIONARIES" SINCE 3.23.00 " - remove")
  endif()
  list(POP_FRONT BD_UNPARSED_ARGUMENTS dictname)
  if(BD_UNPARSED_ARGUMENTS)
    message(
      FATAL_ERROR
        "build_dictionary: non-option arguments not permitted: ${BD_UNPARSED_ARGUMENTS}"
      )
  endif()
  if(DEFINED BD_VERBOSITY AND NOT BD_VERBOSITY MATCHES "^[0-9]+$")
    message(
      FATAL_ERROR "build_dictionary: invalid option VERBOSITY ${BD_VERBOSITY}"
      )
  endif()
  if(NOT TARGET ROOT::Core)
    find_package(ROOT 6.00.00 EXPORT QUIET REQUIRED COMPONENTS Core)
  endif()
  if(NOT dictname)
    cet_package_path(current_subdir)
    string(REPLACE "/" "_" dictname "${current_subdir}")
  endif()
  if(BD_USE_PRODUCT_NAME)
    warn_deprecated(NEW "USE_PROJECT_NAME")
    set(BD_USE_PROJECT_NAME TRUE)
  endif()
  if(BD_USE_PROJECT_NAME)
    string(PREPEND dictname "${CETMODULES_CURRENT_PROJECT_NAME}_")
  endif()
  if(BD_DICT_NAME_VAR)
    set(${BD_DICT_NAME_VAR}
        "${dictname}"
        PARENT_SCOPE
        )
  endif()
  if(BD_SOURCE)
    list(REMOVE_DUPLICATES BD_SOURCE)
    set(xml_files ${BD_SOURCE})
    list(FILTER xml_files INCLUDE REGEX "(^|/)classes_def\\.xml\$")
    list(LENGTH xml_files n_xml_files)
    if(n_xml_files GREATER 1)
      message(
        FATAL_ERROR
          "build_dictionary: SOURCE should specify at most ONE classes_def.xml file"
        )
    endif()
    list(REMOVE_ITEM BD_SOURCE "${xml_files}")
    set(classes_h_files ${BD_SOURCE})
    list(FILTER classes_h_files INCLUDE REGEX "(^|/)classes\\.h\$")
    list(LENGTH classes_h_files n_classes_h_files)
    if(n_classes_h_files GREATER 1)
      message(
        FATAL_ERROR
          "build_dictionary: SOURCE should specify at most ONE classes.h file"
        )
    endif()
    list(REMOVE_ITEM BD_SOURCE "${classes_h_files}")
    set(linkdef_h_files ${BD_SOURCE})
    list(FILTER linkdef_h_files INCLUDE REGEX "(^|/)LinkDef\\.h\$")
    list(LENGTH linkdef_h_files n_linkdef_h_files)
    if(n_linkdef_h_files GREATER 1)
      message(
        FATAL_ERROR
          "build_dictionary: SOURCE should specify at most ONE LinkDef.h file"
        )
    endif()
    list(REMOVE_ITEM BD_SOURCE "${linkdef_h_files}")
  endif()
  if(NOT BD_CLASSES_DEF_XML)
    if(xml_files)
      set(BD_CLASSES_DEF_XML "${xml_files}")
    elseif(NOT BD_SOURCE)
      set(BD_CLASSES_DEF_XML "classes_def.xml")
    endif()
  endif()
  if(BD_CLASSES_DEF_XML AND NOT BD_CLASSES_H)
    if(classes_h_files)
      set(BD_CLASSES_H "${classes_h_files}")
    elseif(NOT BD_SOURCE)
      set(BD_CLASSES_H "classes.h")
    endif()
  endif()
  if(BD_LIB_TARGET)
    set(lib_target_specified "specified ")
  else()
    if(TARGET ${dictname} AND NOT BD_CLASSES_DEF_XML)
      set(BD_LIB_TARGET ${dictname})
    else()
      set(BD_LIB_TARGET "${dictname}_dict")
    endif()
  endif()
  if(BD_SOURCE)
    set(BD_SOURCES_H "${CMAKE_CURRENT_BINARY_DIR}/${BD_LIB_TARGET}_headers.h")
    list(TRANSFORM BD_SOURCE PREPEND "#include \"" OUTPUT_VARIABLE
                                                   BD_SOURCES_H_LINES
         )
    list(TRANSFORM BD_SOURCES_H_LINES APPEND "\"")
    list(JOIN BD_SOURCES_H_LINES "\n" BD_SOURCES_H_CONTENT)
    file(
      GENERATE
      OUTPUT ${BD_SOURCES_H}
      CONTENT "${BD_SOURCES_H_CONTENT}\n" NO_SOURCE_PERMISSIONS
      )
  else()
    set(BD_SOURCES_H)
  endif()
  foreach(item IN ITEMS BD_CLASSES_H BD_CLASSES_DEF_XML linkdef_h_files)
    if(${item})
      cmake_path(ABSOLUTE_PATH ${item})
    endif()
  endforeach()
  if(BD_LIB_TARGET_VAR)
    set(${BD_LIB_TARGET_VAR}
        ${BD_LIB_TARGET}
        PARENT_SCOPE
        )
  endif()
  if(NOT BD_GENERATED_SOURCE_FILENAME)
    set(BD_GENERATED_SOURCE_FILENAME ${BD_LIB_TARGET}.cpp)
  endif()
  cet_process_liblist(
    dictionary_liblist ${BD_LIB_TARGET} PRIVATE ${BD_DICTIONARY_LIBRARIES}
    )
  set(cml_args)
  cet_passthrough(FLAG APPEND BD_NO_INSTALL cml_args)
  cet_passthrough(FLAG APPEND BD_NO_EXPORT cml_args)
  cet_passthrough(APPEND BD_EXPORT_SET cml_args)
  if(NOT (BD_NO_LIBRARY OR TARGET ${BD_LIB_TARGET}))
    # Add library target early; add source later.
    cet_make_library(LIBRARY_NAME ${BD_LIB_TARGET} ${cml_args} SHARED NO_SOURCE)
  endif()
  _generate_dictionary(AUX_OUTPUT_VAR AUX_OUTPUT)
  if(BD_COMPILE_FLAGS)
    set_source_files_properties(
      ${BD_GENERATED_SOURCE_FILENAME} PROPERTIES COMPILE_FLAGS
                                                 ${BD_COMPILE_FLAGS}
      )
  endif()
  if(AUX_OUTPUT AND NOT BD_NO_INSTALL)
    install(FILES ${AUX_OUTPUT}
            DESTINATION ${${CETMODULES_CURRENT_PROJECT_NAME}_LIBRARY_DIR}
            )
  endif()
  if(TARGET ROOT::Core)
    list(APPEND dictionary_liblist PUBLIC ROOT::Core)
  else()
    list(APPEND dictionary_liblist PUBLIC ${ROOT_Core_LIBRARY})
  endif()
  if(NOT TARGET BuildDictionary_AllDicts)
    add_custom_target(BuildDictionary_AllDicts)
  endif()
  if(TARGET ${BD_LIB_TARGET})
    target_link_libraries(${BD_LIB_TARGET} ${dictionary_liblist})
    target_sources(
      ${BD_LIB_TARGET}
      PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/${BD_GENERATED_SOURCE_FILENAME}
      )
    add_dependencies(BuildDictionary_AllDicts ${BD_LIB_TARGET})
  else()
    message(
      FATAL_ERROR
        "${lib_target_specified}target ${BD_LIB_TARGET} is not defined:
1. Ensure that the target name (${BD_LIB_TARGET}) is correct via the LIB_TARGET option.
2. If the target is to be defined elsewhere (as specified by NO_LIBRARY), it must be defined *prior* to the call to build_dictionary().\
"
      )
  endif()
  if(BD_NO_CHECK_CLASS_VERSION OR NOT DEFINED CCV_DEFAULT_RECURSIVE)
    # Turned off manually, or we're using or building an older art.
    if(BD_RECURSIVE
       OR BD_NO_RECURSIVE
       OR BD_CCV_ENVIRONMENT
       )
      message(
        WARNING
          "BuildDictionary: NO_CHECK_CLASS_VERSION is set: CCV_ENVIRONMENT, RECURSIVE AND NO_RECURSIVE are ignored."
        )
    endif()
  elseif(BD_CLASSES_DEF_XML)
    set(BD_CCV_ARGS)
    cet_passthrough(APPEND BD_CLASSES_DEF_XML BD_CCV_ARGS)
    cet_passthrough(FLAG APPEND BD_RECURSIVE BD_CCV_ARGS)
    cet_passthrough(FLAG APPEND BD_NO_RECURSIVE BD_CCV_ARGS)
    cet_passthrough(APPEND KEYWORD ENVIRONMENT BD_CCV_ENVIRONMENT BD_CCV_ARGS)
    check_class_version(${BD_LIBRARIES} UPDATE_IN_PLACE ${BD_CCV_ARGS})
  endif()
endfunction()

function(_generate_dictionary)
  cmake_parse_arguments(PARSE_ARGV 0 GD "" "AUX_OUTPUT_VAR" "")
  set(generate_dictionary_usage
      "_generate_dictionary( [DICT_FUNCTIONS] [dictionary_name] )"
      )
  # Add target-specific include directories and compile definitions, accounting
  # safely for generator expressions.
  set(tmp_defs "$<TARGET_PROPERTY:${BD_LIB_TARGET},COMPILE_DEFINITIONS>")
  set(ROOTCLING_DEFS
      "$<$<BOOL:${tmp_defs}>:-D$<JOIN:${tmp_defs},$<SEMICOLON>-D>>"
      )
  set(tmp_includes "$<TARGET_PROPERTY:${BD_LIB_TARGET},INCLUDE_DIRECTORIES>")
  set(target_includes
      "$<$<BOOL:${tmp_includes}>:-I$<JOIN:${tmp_includes},$<SEMICOLON>-I>>"
      )
  get_property(
    dir_includes
    DIRECTORY
    PROPERTY INCLUDE_DIRECTORIES
    )
  list(TRANSFORM dir_includes PREPEND "-I")
  if(BD_VERBOSITY)
    set(rootcling_vflag -v${BD_VERBOSITY})
  elseif(BD_VERBOSE)
    set(rootcling_vflag -v4)
  else()
    set(rootcling_vflag -v2)
  endif()
  set(ROOTCLING_FLAGS
      -f ${rootcling_vflag}
      ${CMAKE_CURRENT_BINARY_DIR}/${BD_GENERATED_SOURCE_FILENAME}
      --failOnWarnings --noGlobalUsingStd --noIncludePaths
      )
  if(BD_CLASSES_DEF_XML OR BD_SOURCES_H)
    list(APPEND ROOTCLING_FLAGS --inlineInputHeader)
  endif()
  if(BD_CLASSES_DEF_XML)
    list(PREPEND ROOTCLING_FLAGS --reflex)
  else()
    set(BD_NO_CHECK_CLASS_VERSION TRUE)
  endif()
  list(APPEND ROOTCLING_FLAGS -I${CETMODULES_CURRENT_PROJECT_SOURCE_DIR}
       ${target_includes} ${dir_includes} ${ROOTCLING_DEFS}
       )
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/module.modulemap")
    set(ROOTCLING_MODULEMAP "${CMAKE_CURRENT_BINARY_DIR}/module.modulemap")
    cet_copy("${CMAKE_CURRENT_SOURCE_DIR}/module.modulemap" DESTINATION
             "${CMAKE_CURRENT_BINARY_DIR}"
             )
    list(APPEND ROOTCLING_FLAGS --cxxmodule)
    set(AUX_OUTPUT
        "$<TARGET_FILE_DIR:${BD_LIB_TARGET}>/$<TARGET_FILE_BASE_NAME:${BD_LIB_TARGET}>.pcm"
        )
  else()
    unset(ROOTCLING_MODULEMAP)
    set(AUX_OUTPUT
        "$<TARGET_FILE_DIR:${BD_LIB_TARGET}>/$<TARGET_FILE_PREFIX:${BD_LIB_TARGET}>$<TARGET_FILE_BASE_NAME:${BD_LIB_TARGET}>.rootmap"
        )
    if(NOT ROOT_VERSION GREATER_EQUAL 6.10.04 AND CMAKE_SYSTEM_NAME MATCHES
                                                  "Darwin"
       )
      # Header line and OS X lib name fixing in .rootmap only necessary for
      # older ROOT6.
      add_custom_command(
        TARGET ${BD_LIB_TARGET}
        POST_BUILD
        COMMAND perl -wapi.bak -e s&\\.dylib\\.so&.dylib&g ${AUX_OUTPUT}
        COMMAND rm -f ${AUX_OUTPUT}.bak
        COMMENT Fixing shared library reference in ${AUX_OUTPUT}
        VERBATIM
        )
    endif()
    list(APPEND ROOTCLING_FLAGS --rmf=${AUX_OUTPUT}
         --rml="$<TARGET_FILE_NAME:${BD_LIB_TARGET}>"
         )
  endif()
  list(APPEND ROOTCLING_FLAGS -s "$<TARGET_LINKER_FILE:${BD_LIB_TARGET}>")
  list(
    APPEND
    AUX_OUTPUT
    "$<TARGET_FILE_DIR:${BD_LIB_TARGET}>/$<TARGET_FILE_PREFIX:${BD_LIB_TARGET}>$<TARGET_FILE_BASE_NAME:${BD_LIB_TARGET}>_rdict.pcm"
    )
  if(GD_AUX_OUTPUT_VAR)
    set(${GD_AUX_OUTPUT_VAR}
        ${AUX_OUTPUT}
        PARENT_SCOPE
        )
  endif()
  set(implicit_depends)
  foreach(item IN LISTS BD_CLASSES_H BD_SOURCES_H)
    list(APPEND implicit_depends CXX ${item})
  endforeach()
  add_custom_command(
    # See https://gitlab.kitware.com/cmake/cmake/-/issues/21364#note_849331
    OUTPUT
      ${CMAKE_CURRENT_BINARY_DIR}/${BD_GENERATED_SOURCE_FILENAME} # ${AUX_OUTPUT}
    COMMAND ROOT::rootcling ${ROOTCLING_FLAGS} ${BD_CLASSES_H}
            ${BD_CLASSES_DEF_XML} ${BD_SOURCES_H} ${linkdef_h_files}
    IMPLICIT_DEPENDS ${implicit_depends}
    DEPENDS ${BD_CLASSES_DEF_XML} ${ROOTCLING_MODULEMAP}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMAND_EXPAND_LISTS
    COMMENT "Generating dictionary files for target ${dictname}"
    )
  # set variable for install_source
  set(cet_generated_code
      ${CMAKE_CURRENT_BINARY_DIR}/${BD_GENERATED_SOURCE_FILENAME}
      PARENT_SCOPE
      )
  if(ROOTCLING_MODULEMAP AND NOT BD_NO_INSTALL)
    # We need to concatenate all module.modulemap files destined for
    # installation in the same header directory.
    if(NOT BD_MODULEMAP_INSTALL_DIR)
      set(BD_MODULEMAP_INSTALL_DIR
          "${${CETMODULES_CURRENT_PROJECT_NAME}_INCLUDE_DIR}"
          )
    endif()
    string(
      MAKE_C_IDENTIFIER "BuildDictionary_modulemap_${BD_MODULEMAP_INSTALL_DIR}"
                        MODULEMAP_CONSOLIDATION_TARGET
      )
    cet_package_path(current_subdir)
    string(MAKE_C_IDENTIFIER "${current_subdir}" subdir_identifier)
    cet_copy(
      ${ROOTCLING_MODULEMAP}
      NO_ALL
      NAME
      ${subdir_identifier}.modulemap
      TARGET_VAR
      modulemap_target
      DESTINATION
      "${PROJECT_BINARY_DIR}/${MODULEMAP_CONSOLIDATION_TARGET}"
      )
    if(NOT TARGET ${MODULEMAP_CONSOLIDATION_TARGET})
      add_custom_target(${MODULEMAP_CONSOLIDATION_TARGET} ALL)
      # Two-tier target ensures all modulemap files are copied to the temporary
      # directory before we attempt to concatenate them all.
      add_custom_command(
        OUTPUT
          "${PROJECT_BINARY_DIR}/${MODULEMAP_CONSOLIDATION_TARGET}.modulemap"
        COMMAND
          ${CMAKE_COMMAND} -E cat --
          ${PROJECT_BINARY_DIR}/${MODULEMAP_CONSOLIDATION_TARGET}/*.modulemap >
          ${MODULEMAP_CONSOLIDATION_TARGET}.modulemap
        WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
        COMMAND_EXPAND_LISTS
        )
      add_custom_target(
        ${MODULEMAP_CONSOLIDATION_TARGET}_CAT ALL
        DEPENDS
          "${PROJECT_BINARY_DIR}/${MODULEMAP_CONSOLIDATION_TARGET}.modulemap"
        )
      add_dependencies(
        ${MODULEMAP_CONSOLIDATION_TARGET}_CAT ${MODULEMAP_CONSOLIDATION_TARGET}
        )
      install(
        FILES
          "${PROJECT_BINARY_DIR}/${MODULEMAP_CONSOLIDATION_TARGET}.modulemap"
        DESTINATION "${BD_MODULEMAP_INSTALL_DIR}"
        RENAME module.modulemap
        )
    endif()
    add_dependencies(${MODULEMAP_CONSOLIDATION_TARGET} ${modulemap_target})
  endif()
endfunction()
