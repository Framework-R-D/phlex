#[================================================================[.rst:
BasicPlugin
-----------

Module defining commands to facilitate the building of plugins:

* :command:`basic_plugin`
* :command:`cet_build_plugin`
* :command:`cet_collect_plugin_builders`
* :command:`cet_make_plugin_builder`
* :command:`cet_write_plugin_builder`

#]================================================================]

# Avoid unnecessary repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

include(CetCMakeUtils)
include(CetMakeLibrary)
include(CetPackagePath)
include(CetRegexEscape)

set(cet_bp_flags
    ALLOW_UNDERSCORES
    BASENAME_ONLY
    NOP
    NO_EXPORT
    NO_INSTALL
    USE_BOOST_UNIT
    USE_PROJECT_NAME
    USE_PRODUCT_NAME
    VERSION
    )
set(cet_bp_one_arg_opts EXPORT_SET IMPL_TARGET_VAR SOVERSION)
set(cet_bp_list_options ALIAS IMPL_SOURCE LIBRARIES LOCAL_INCLUDE_DIRS
                        REG_SOURCE SOURCE
    )

cet_regex_escape(
  ${cet_bp_flags} ${cet_bp_one_arg_opts} ${cet_bp_list_options} VAR _e_bp_args
  )
string(REPLACE ";" "|" _e_bp_args "${_e_bp_args}")

#[================================================================[.rst:
.. |ODR| replace:: :abbr:`ODR (One Definition Rule)`

.. command:: basic_plugin

   Create a plugin library, with or without a separate registration
   library to avoid |ODR| violations.

   .. code-block:: cmake

      basic_plugin(<name> <suffix> [<options>])

   Options
   ^^^^^^^

   Source specification options
   """"""""""""""""""""""""""""

   ``IMPL_SOURCE <implementation-source>...``
     Specify source to compile into the plugin's interface
     implementation library, if appropriate. The implementation should
     *not* invoke any registration definition macros or the |ODR| will
     be violated.

   ``REG_SOURCE <registration-source>...``
     Specify source to compile into the plugin registration library,
     intended only for runtime injection (via e.g. :manpage:`dlopen(3)`)
     into an executable, and not for dynamic linking.

   .. note::

      * If ``REG_SOURCE`` is omitted, we look for ``<name>_<suffix>.cc``

      * If ``IMPL_SOURCE`` is omitted, we look for ``<name>.cc``

   Dependency specification options
   """"""""""""""""""""""""""""""""

   ``LIBRARIES [CONDITIONAL|INTERFACE|PRIVATE|PROTECTED|PUBLIC|REG]
   <library-dependency>...``
     Specify targets and/or libraries upon which the implementation
     (``INTERFACE``, ``PUBLIC``, ``PRIVATE``), or registration (``REG``)
     libraries should depend. If implementation and registration share a
     dependency not inherited by public callers of the implementation,
     specify the library twice with one mention prefaced with
     ``PRIVATE`` and the other with ``REG``.

   .. note::

      * The registration library has an automatic dependence on the
        implementation library (if present).

      * If ``PUBLIC`` or ``INTERFACE`` dependencies are specified and
        there is no implementation source, then the plugin will be built
        as a shared library rather than as a module, and all
        responsibility for |ODR| violations rests with the plugin
        builder.

      * An additional dependency designation, ``CONDITIONAL``, is
        accepted and is intended for use by intermediate CMake functions
        that add dependencies to a library. ``CONDITIONAL`` is identical
        to ``PUBLIC`` without making a statement about the shared or
        module nature of the combined implementation/registration
        library or the presence of a public (non-plugin) calling
        interface.

   Other options
   """""""""""""

   ``ALIAS <alias>...``
     Create the specified CMake alias targets to the implementation
     library.

   ``ALLOW_UNDERSCORES``
     Normally, neither ``<name>`` nor ``<suffix>`` may contain
     underscores in order to avoid possible ambiguities. Allow them with
     this option at your own risk.

   ``BASENAME_ONLY``
     Do not add the relative path (directories delimited by ``_``) to
     the front of the plugin library name.

   ``EXPORT_SET <export-name>``
     Add the library to the ``<export-name>`` export set.

   ``IMPL_TARGET_VAR <var>``
     Return the—possibly calculated—name of the implementation library
     target in ``<var>``.

   ``LOCAL_INCLUDE_DIRS <dir>...``
     Headers may be found in ``<dir>``... at build time.

   ``NOP``
     Option / argument disambiguator; no other function.

   ``NO_EXPORT``
     Do not export this plugin.

   ``NO_INSTALL``
     Do not install the generated library or libraries.

   ``SOVERSION <version>``
     The library's compatibility version (*cf* CMake
     :prop_tgt:`SOVERSION <cmake-ref-current:prop_tgt:SOVERSION>`
     property).

   ``USE_BOOST_UNIT``
     The plugin uses `Boost unit test functions
     <https://www.boost.org/doc/libs/release/libs/test/doc/html/index.html>`_
     and should be compiled and linked accordingly.

   ``USE_PROJECT_NAME``
     .. versionadded:: 3.23.00

     The project name will be prepended to the plugin library name,
     separated by ``_``

   ``VERSION``
     The library's build version will be set to
     :variable:`CETMODULES_CURRENT_PROJECT_VERSION` (*cf* CMake
     :prop_tgt:`VERSION <cmake-ref-current:prop_tgt:VERSION>` property).

   Deprecated options
   """"""""""""""""""

   ``SOURCE <source>...``
     Specify sources to compile into the plugin.

    .. deprecated:: 2.11 use ``IMPL_SOURCE``, ``REG_SOURCE`` and
       ``LIBRARIES REG`` instead.

   ``USE_PRODUCT_NAME``
     .. deprecated:: 2.0 use ``USE_PROJECT_NAME`` instead.

   Non-option arguments
   """"""""""""""""""""

   ``<name>``
     The name stem for the library to be generated.

   ``<suffix>``
     The category of plugin to be generated.

   .. seealso:: :command:`cet_make_library`

#]================================================================]

function(basic_plugin NAME SUFFIX)
  cmake_parse_arguments(
    PARSE_ARGV 2 BP "${cet_bp_flags}" "${cet_bp_one_arg_opts}"
    "${cet_bp_list_options}"
    )
  if(BP_UNPARSED_ARGUMENTS)
    warn_deprecated(
      "use of extra non-option arguments (${BP_UNPARSED_ARGUMENTS})" NEW
      "LIBRARIES"
      )
    list(APPEND BP_LIBRARIES NOP ${BP_UNPARSED_ARGUMENTS})
  endif()
  if(BP_USE_PRODUCT_NAME)
    warn_deprecated(NEW "USE_PROJECT_NAME")
    set(BP_USE_PROJECT_NAME TRUE)
  endif()
  if(BP_BASENAME_ONLY)
    set(plugin_stem "${NAME}")
  else()
    cet_package_path(CURRENT_SUBDIR)
    if(NOT BP_ALLOW_UNDERSCORES)
      if(CURRENT_SUBDIR MATCHES _)
        message(
          FATAL_ERROR
            "found underscore in plugin subdirectory: ${CURRENT_SUBDIR}"
          )
      endif()
      if(NAME MATCHES _)
        message(FATAL_ERROR "found underscore in plugin name: ${NAME}")
      endif()
    endif()
    string(REPLACE "/" "_" plugin_stem "${CURRENT_SUBDIR}")
    string(JOIN "_" plugin_stem "${plugin_stem}" "${NAME}")
  endif()
  if(BP_USE_PROJECT_NAME)
    string(JOIN "_" plugin_stem "${CETMODULES_CURRENT_PROJECT_NAME}"
           "${plugin_stem}"
           )
  endif()
  if(BP_SOURCE)
    warn_deprecated("SOURCE" NEW "IMPL_SOURCE, REG_SOURCE and LIBRARIES REG")
    if(BP_REG_SOURCE)
      message(FATAL_ERROR "SOURCE and REG_SOURCE are mutually exclusive")
    endif()
    set(BP_REG_SOURCE "${BP_SOURCE}")
  elseif(NOT BP_REG_SOURCE)
    set(BP_REG_SOURCE "${NAME}_${SUFFIX}.cc")
  endif()
  set(cml_args)
  # ############################################################################
  # These items are common to implementation and plugin libraries.
  foreach(kw IN ITEMS EXPORT_SET LOCAL_INCLUDE_DIRS SOVERSION)
    cet_passthrough(APPEND BP_${kw} cml_common_args)
  endforeach()
  foreach(kw IN ITEMS NO_INSTALL VERSION)
    cet_passthrough(FLAG APPEND BP_${kw} cml_common_args)
  endforeach()
  # These items are only for the implementation library.
  foreach(kw IN ITEMS ALIAS)
    cet_passthrough(APPEND BP_${kw} cml_impl_args)
  endforeach()
  foreach(kw IN ITEMS NO_EXPORT USE_BOOST_UNIT)
    cet_passthrough(FLAG APPEND BP_${kw} cml_impl_args)
  endforeach()
  # ############################################################################
  set(target_thunk)
  cmake_parse_arguments(
    BPL "NOP" "" "CONDITIONAL;INTERFACE;PRIVATE;PUBLIC;REG" ${BP_LIBRARIES}
    )
  list(APPEND BPL_PUBLIC ${BPL_UNPARSED_ARGUMENTS})
  if(NOT BP_IMPL_SOURCE) # See if we can find one.
    get_filename_component(if_plugin_impl "${NAME}.cc" REALPATH)
    if(EXISTS "${if_plugin_impl}")
      set(BP_IMPL_SOURCE "${NAME}.cc")
    endif()
  endif()
  if(BP_IMPL_SOURCE
     OR "IMPL_SOURCE" IN_LIST BP_KEYWORDS_MISSING_VALUES
     OR NOT
        (BPL_INTERFACE
         OR BPL_PUBLIC
         OR BPL_KEYWORDS_MISSING_VALUES MATCHES "(^|;)(INTERFACE|PUBLIC)(;|$)")
     )
    set(REG_LIB_TYPE MODULE)
    if(BP_IMPL_SOURCE OR "IMPL_SOURCE" IN_LIST BP_KEYWORDS_MISSING_VALUES)
      if(BP_IMPL_SOURCE)
        list(APPEND BPL_PUBLIC ${BPL_CONDITIONAL})
        list(PREPEND BP_IMPL_SOURCE "SOURCE")
      else()
        list(APPEND BPL_INTERFACE ${BPL_CONDITIONAL})
        set(BP_IMPL_SOURCE NO_SOURCE INTERFACE)
      endif()
      list(REMOVE_DUPLICATES BPL_INTERFACE)
      list(REMOVE_DUPLICATES BPL_PUBLIC)
      cet_make_library(
        LIBRARY_NAME
        "${plugin_stem}_${SUFFIX}"
        ${BP_IMPL_SOURCE}
        LIBRARIES
        INTERFACE
        ${BPL_INTERFACE}
        PUBLIC
        ${BPL_PUBLIC}
        PRIVATE
        ${BPL_PRIVATE}
        NOP
        ${cml_common_args}
        ${cml_impl_args}
        )
      # For backward compatibility purposes, we retain the vanilla target name
      # but have a different name for the implementation library on disk.
      set_target_properties(
        "${plugin_stem}_${SUFFIX}" PROPERTIES OUTPUT_NAME "${plugin_stem}"
        )
      if(BP_IMPL_TARGET_VAR)
        set(${BP_IMPL_TARGET_VAR}
            "${plugin_stem}_${SUFFIX}"
            PARENT_SCOPE
            )
      endif()
      # Thunk the target name of the plugin library so we don't attempt to link
      # to it, but retain the vanilla library name for backward compatibility.
      set(target_thunk _reg)
      # Trim the library list for the registration library:
      set(BP_LIBRARIES PRIVATE "${plugin_stem}_${SUFFIX}" ${BPL_REG})
      unset(cml_impl_args)
    else() # One combined module that we've been told is safe:
      set(BP_LIBRARIES PRIVATE ${BPL_CONDITIONAL} ${BPL_PRIVATE} ${BPL_REG})
    endif()
  else()
    set(REG_LIB_TYPE SHARED)
    list(APPEND BPL_PUBLIC ${BPL_CONDITIONAL})
    list(REMOVE_DUPLICATES BPL_PUBLIC)
    list(APPEND BPL_PRIVATE ${BPL_REG})
    list(REMOVE_DUPLICATES BPL_PRIVATE)
    set(BP_LIBRARIES INTERFACE ${BPL_INTERFACE} PUBLIC ${BPL_PUBLIC} PRIVATE
                     ${BPL_PRIVATE}
        )
    if(CET_WARN_DEPRECATED)
      message(
        AUTHOR_WARNING
          "prefer separate compilation units for implementation (IMPL_SOURCE) and plugin registration macros (REG_SOURCE, LIBRARIES REG) due to possible consequences of One Definition Rule violation"
        )
    endif()
  endif()
  # ############################################################################
  # Make the plugin library, to which we should not normally link directly (see
  # REG_SOURCE, above).
  #
  # Module-type libraries containing only plugin registration code can be
  # stripped.
  cet_passthrough(IN_PLACE BP_REG_SOURCE KEYWORD SOURCE EMPTY_KEYWORD NO_SOURCE)
  cet_passthrough(FLAG APPEND target_thunk KEYWORD STRIP_LIBS cml_impl_args)
  if(REG_LIB_TYPE STREQUAL "MODULE" AND NOT NO_INSTALL)
    # We don't want the plugin-only library visible as an exported target.
    list(APPEND cml_impl_args NO_EXPORT)
  endif()
  cet_make_library(
    LIBRARY_NAME
    "${plugin_stem}_${SUFFIX}${target_thunk}"
    ${REG_LIB_TYPE}
    ${BP_REG_SOURCE}
    ${cml_common_args}
    ${cml_impl_args}
    LIBRARIES
    ${BP_LIBRARIES}
    )
  if(target_thunk)
    set_target_properties(
      ${plugin_stem}_${SUFFIX}${target_thunk}
      PROPERTIES OUTPUT_NAME "${plugin_stem}_${SUFFIX}"
      )
  elseif(BP_IMPL_TARGET_VAR)
    set(${BP_IMPL_TARGET_VAR}
        "${plugin_stem}_${SUFFIX}${target_thunk}"
        PARENT_SCOPE
        )
  endif()
endfunction()

#[================================================================[.rst:
.. command:: cet_build_plugin

   Build a plugin of a specific type.

   .. code-block:: cmake

      cet_build_plugin(<name> <base> <arg> ...)

   Details
   ^^^^^^^

   \ :command:`!cet_build_plugin` attempts to locate a command to invoke
   to build a plugin ``<name>`` of type ``<base>``.

   If there exists a CMake variable :variable:`!<unscoped-base>_builder`
   (where ``<unscoped-base>`` is ``<base>`` after stripping any
   namespace prefix (``*::``), the first element of its value will be
   searched for as a command to invoke, and any further elements will be
   prepended to ``<arg> ...``. Otherwise, a command ``<base>`` or
   ``<base>_plugin`` will be invoked if found.

   If a suitable command :command:`!<cmd>` is found, it shall be
   invoked:

   .. code-block:: cmake

      <cmd>(<name> <args>)

   If no suitable command is found but there exists a CMake variable
   :variable:`!<base>_LIBRARIES`, the following command shall be
   invoked:

   .. code-block:: cmake

      basic_plugin(<name> <base> LIBRARIES ${<base>_LIBRARIES} <arg> ...)

#]================================================================]

macro(cet_build_plugin NAME BASE)
  if("${BASE}" STREQUAL "")
    message(SEND_ERROR "vacuous BASE argument to cet_build_plugin()")
  else()
    string(REGEX REPLACE "^.*::" "" base_varstem "${BASE}")
    foreach(_cbp_command IN ITEMS "${${base_varstem}_builder}" "${BASE}"
                                  "${BASE}_plugin"
            )
      list(POP_FRONT _cbp_command _cbp_cmd_name)
      if(COMMAND ${_cbp_cmd_name})
        list(PREPEND _cbp_cmd_names ${_cbp_cmd_name}) # Handle recursion.
        cmake_language(CALL ${_cbp_cmd_name} ${NAME} ${_cbp_command} ${ARGN})
        list(POP_FRONT _cbp_cmd_names _cbp_cmd_name)
        break()
      endif()
      unset(_cbp_cmd_name)
    endforeach()
    unset(_cbp_command)
    if(_cbp_cmd_name)
      unset(_cbp_cmd_name)
    elseif(DEFINED ${BASE}_LIBRARIES)
      basic_plugin(${NAME} ${BASE} LIBRARIES ${${BASE}_LIBRARIES} ${ARGN})
    else()
      message(
        SEND_ERROR
          "unable to find plugin builder for plugin type \"${BASE}\": missing include()?
Need ${BASE}(), ${BASE}_plugin() or dependencies in \${${BASE}_LIBRARIES}, or use basic_plugin()"
        )
    endif()
  endif()
endmacro()

#[================================================================[.rst:
.. command:: cet_collect_plugin_builders

   Generate and install a CMake wrapper file to include plugin builders.

   .. code-block:: cmake

      cet_collect_plugin_builders(<dest-subdir> [<name>] [<options>])

   Options
   ^^^^^^^

   .. _cet_collect_plugin_builders-opt-LIST:

   ``LIST <type> ...``
     Specify explicit builders to include; ``<name>`` is required.

   ``NOP``
     Option / argument disambiguator; no other function.

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<dest-subdir>``
     Destination for the generated CMake file.

   ``<name>``
     The basename (without extension) of the generated CMake
     file. Required if :ref:`cet_collect_plugin_builders-opt-LIST` is
     present; otherwise defaults to
     :variable:`${CETMODULES_CURRENT_PROJECT_NAME}
     <CETMODULES_CURRENT_PROJECT_NAME>`\ :file:`PluginBuilders`.

   Details
   ^^^^^^^

   Generate a CMake file :variable:`${PROJECT_BINARY_DIR}
   <cmake-ref-current:variable:PROJECT_BINARY_DIR>`\
   :file:`/<dest-subdir>/<name>.cmake` which includes generated plugin
   builders ``<type> ...`` specified by
   :ref:`cet_collect_plugin_builders-opt-LIST` if present; otherwise all
   those generated by :command:`cet_make_plugin_builder` since
   :command:`cet_collect_plugin_builders` was called last.

#]================================================================]

function(cet_collect_plugin_builders DEST_SUBDIR)
  cmake_parse_arguments(PARSE_ARGV 1 _ccpb "NOP" "" "LIST")
  list(POP_FRONT _ccpb_UNPARSED_ARGUMENTS NAME_WE)
  if("${NAME_WE}" STREQUAL "")
    if(NOT "${_ccpb_LIST}" STREQUAL "")
      message(FATAL_ERROR "wrapper filepath required when LIST is specified")
    endif()
    set(NAME_WE ${CETMODULES_CURRENT_PROJECT_NAME}PluginBuilders)
  endif()
  if("${_ccpb_LIST}" STREQUAL "")
    set(_ccpb_LIST
        "${CETMODULES_PLUGIN_BUILDERS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}}"
        )
    unset(CETMODULES_PLUGIN_BUILDERS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
          PARENT_SCOPE
          )
    unset(CETMODULES_PLUGIN_BUILDERS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
          CACHE
          )
  endif()
  list(SORT _ccpb_LIST)
  list(TRANSFORM _ccpb_LIST REPLACE "^(.+)$" "include(\\1)" OUTPUT_VARIABLE
                                                            _ccpb_includes
       )
  list(JOIN _ccpb_includes "\n" _ccpb_includes_content)
  file(
    WRITE
    "${${CETMODULES_CURRENT_PROJECT_NAME}_BINARY_DIR}/${DEST_SUBDIR}/${NAME_WE}.cmake"
    "\
include_guard()

${_ccpb_includes_content}
\
"
    )
  install(
    FILES
      "${${CETMODULES_CURRENT_PROJECT_NAME}_BINARY_DIR}/${DEST_SUBDIR}/${NAME_WE}.cmake"
    DESTINATION "${DEST_SUBDIR}"
    )
endfunction()

#[================================================================[.rst:
.. command:: cet_make_plugin_builder

   Generate a plugin builder function using
   :command:`cet_write_plugin_builder` and register it for collection by
   :command:`cet_collect_plugin_builders`.

   ..  code-block:: cmake

       cet_make_plugin_builder(<type> <base> <dest-subdir> [<options>] <args>)

   .. seealso:: :command:`cet_write_plugin_builder`

#]================================================================]

function(cet_make_plugin_builder TYPE BASE DEST_SUBDIR)
  cet_write_plugin_builder(${ARGV} INSTALL_BUILDER)
  if(NOT
     DEFINED
     CACHE{CETMODULES_PLUGIN_BUILDERS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}}
     )
    set(CETMODULES_PLUGIN_BUILDERS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
        CACHE
          INTERNAL
          "CMake modules defining plugin builders for project ${CETMODULES_CURRENT_PROJECT_NAME}"
        )
  endif()
  set_property(
    CACHE CETMODULES_PLUGIN_BUILDERS_PROJECT_${CETMODULES_CURRENT_PROJECT_NAME}
    APPEND
    PROPERTY VALUE "${TYPE}"
    )
endfunction()

#[================================================================[.rst:
.. command:: cet_write_plugin_builder

   Generate a plugin builder function using
   :command:`cet_write_plugin_builder`.

   ..  code-block:: cmake

       cet_write_plugin_builder(<type> <base> <dest-subdir> [<options>] <args>)

   Options
   ^^^^^^^

   ``INSTALL_BUILDER``
     The generated file shall be installed in
     :variable:`${CMAKE_INSTALL_PREFIX}
     <cmake-ref-current:variable:CMAKE_INSTALL_PREFIX>`\
     :file:`/<dest-subdir>`.

   ``NOP``
     Option / argument disambiguator; no other function.

   ``SUFFIX <suffix>``
     .. seealso:: :command:`basic_plugin`.

   Details
   ^^^^^^^

   Generate a file :variable:`${PROJECT_BINARY_DIR}
   <cmake-ref-current:variable:PROJECT_BINARY_DIR>`\
   :file:`/<dest-subdir>/<type>.cmake` defining a function:

   .. code-block:: cmake

      macro(<type> NAME)
        <func>(${NAME} <base-ish> ${ARGN} <args>)
      endmacro()

   If ``<type>`` and ``<base>`` are the same:
     * ``<func>`` is :command:`basic_plugin`
     * ``<base-ish>`` is ``<suffix>`` if specified, or ``<base>`` after
       stripping any namespace prefix (``*::``).

   Otherwise:
     * ``<func>`` is :command:`cet_build_plugin`
     * ``<base-ish>`` is ``<suffix>`` if specified, or ``<base>``.

#]================================================================]

# This macro will generate a CMake builder function for plugins of type (e.g.
# inheriting from) TYPE.
function(cet_write_plugin_builder TYPE BASE DEST_SUBDIR)
  cmake_parse_arguments(PARSE_ARGV 3 _cwpb "INSTALL_BUILDER;NOP" "SUFFIX" "")
  # Allow a layered hierarchy while preventing looping.
  if(TYPE STREQUAL BASE)
    if("${_cwpb_SUFFIX}" STREQUAL "")
      string(REGEX REPLACE "^.*::" "" BASE_ARG "${BASE}")
    else()
      set(BASE_ARG "${_cwpb_SUFFIX}")
    endif()
    set(build basic)
    set(extra_includes)
  else()
    set(build cet_build)
    set(extra_includes "include(${BASE})\n")
    if(DEFINED _cwpb_SUFFIX)
      set(BASE_ARG ${BASE} SUFFIX ${_cwpb_SUFFIX})
    else()
      set(BASE_ARG ${BASE})
    endif()
  endif()
  file(
    WRITE
    "${${CETMODULES_CURRENT_PROJECT_NAME}_BINARY_DIR}/${DEST_SUBDIR}/${TYPE}.cmake"
    "\
include_guard()
cmake_minimum_required(VERSION 3.18...4.1 FATAL_ERROR)

${extra_includes}include(BasicPlugin)

# Generate a CMake plugin builder macro for tools of type ${TYPE} for
# automatic invocation by build_plugin().
macro(${TYPE} NAME)
  ${build}_plugin(\${NAME} ${BASE_ARG} \${ARGN} ${_cwpb_UNPARSED_ARGUMENTS})
endmacro()
\
"
    )
  if(_cwpb_INSTALL_BUILDER)
    install(
      FILES
        "${${CETMODULES_CURRENT_PROJECT_NAME}_BINARY_DIR}/${DEST_SUBDIR}/${TYPE}.cmake"
      DESTINATION "${DEST_SUBDIR}"
      )
  endif()
endfunction()
