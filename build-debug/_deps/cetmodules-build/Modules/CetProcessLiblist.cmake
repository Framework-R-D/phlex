#[================================================================[.rst:
X
-
#]================================================================]

# Avoid unnecessary repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

function(cet_process_liblist RESULT_VAR)
  cet_convert_target_args(${ARGV})
  set(${RESULT_VAR}
      "${${RESULT_VAR}}"
      PARENT_SCOPE
      )
endfunction()

function(cet_convert_target_args RESULT_VAR DEP_TARGET)
  set(RESULTS)
  set(scope PRIVATE)
  foreach(arg IN LISTS ARGN)
    if(arg MATCHES "^(INTERFACE|PRIVATE|PUBLIC)$")
      set(scope "${arg}")
    elseif(NOT arg MATCHES "(^((debug|general|optimized)$|-|\\$<))|/")
      # Could be a target, a variable or a literal library.
      _cet_convert_target_arg("${arg}" arg)
    endif()
    list(APPEND RESULTS "${arg}")
  endforeach()
  set(${RESULT_VAR}
      "${RESULTS}"
      PARENT_SCOPE
      )
endfunction()

function(_cet_convert_target_arg ARG RESULT_VAR)
  set(DOLLAR "@CET_DOLLAR@")
  # Can we convert it to an uppercase variable we can substitute?
  string(TOUPPER "${ARG}" ${ARG}_UC)
  set(RESULT)
  if(DEFINED ${${ARG}_UC})
    if(${${ARG}_UC} MATCHES ";" OR NOT ${${ARG}_UC} MATCHES "/")
      # Possibly requiring further expansion:
      set(tmp "${${${ARG}_UC}}")
      if("${tmp}" STREQUAL ""
         OR "${tmp}" STREQUAL "${ARG}"
         OR "${tmp}" STREQUAL "${${ARG}_UC}"
         )
        # prevent cycles
        set(${RESULT_VAR}
            "${tmp}"
            PARENT_SCOPE
            )
        return()
      endif()
      cet_convert_target_args(RESULT ${DEP_TARGET} "${tmp}")
    else()
      # Delay expansion for variables resolving to paths.
      if(NOT scope STREQUAL INTERFACE)
        if(scope STREQUAL PUBLIC)
          list(APPEND RESULT PRIVATE)
        endif()
        list(APPEND RESULT "${${${ARG}_UC}}")
      endif()
      if(NOT scope STREQUAL PRIVATE)
        list(APPEND RESULT INTERFACE "$<BUILD_INTERFACE:${${${ARG}_UC}}>"
             "$<INSTALL_INTERFACE:${DOLLAR}{${${ARG}_UC}}>" ${scope}
             )
      endif()
    endif()
  endif()
  if("${RESULT}" STREQUAL "")
    if(TARGET "${ARG}") # We already know it's a target: check is
                        # straightforward.
      get_target_property(target_type "${ARG}" TYPE)
      if(target_type STREQUAL "MODULE_LIBRARY")
        message(
          SEND_ERROR
            "
target ${DEP_TARGET} cannot link to target ${ARG} of CMake type \"${target_type}\".
Separate implementation from plugin registration code (e.g. X.cc vs \
X_<plugin-suffix>.cc) (strongly recommended), specify PUBLIC dependencies to \
basic_plugin() (temporary solution only), or set \
${CETMODULES_CURRENT_PROJECT_NAME}_MODULE_PLUGINS to FALSE (not recommended)\
"
          )
      elseif(NOT target_type MATCHES "_LIBRARY$")
        message(FATAL_ERROR "target ${ARG} has unexpected type ${target_type}")
      endif()
      set(RESULT "${ARG}")
    else() # Might be a target which has not been defined yet.
      # Put mechanisms in place to catch link problems at link time if we can't
      # detect them earlier (see 'if (TARGET "${arg}") ...' above).
      if(NOT scope STREQUAL INTERFACE)
        string(MAKE_C_IDENTIFIER "${DEP_TARGET}-${ARG}>-ERROR.txt" error_file)
        set(dollar "$<$<TARGET_EXISTS:${ARG}>:$>")
        set(dependency_type "${dollar}<TARGET_PROPERTY:${ARG},TYPE$<ANGLE-R>")
        set(gen_condition
            "$<BOOL:$<$<TARGET_EXISTS:${ARG}>:$<GENEX_EVAL:${dollar}<STREQUAL:MODULE_LIBRARY,${dependency_type}$<ANGLE-R>>>>"
            )
        set(link_argument
            "$<IF:${gen_condition},UNLINKABLE-MODULE-LIBRARY-TARGET-SEE-${CMAKE_CURRENT_BINARY_DIR}/${error_file},${ARG}>"
            )
        if(CMAKE_MESSAGE_LOG_LEVEL MATCHES "^(VERBOSE|DEBUG|TRACE)$")
          string(MAKE_C_IDENTIFIER "${DEP_TARGET}-${ARG}>-type.txt" type_file)
          file(
            GENERATE
            OUTPUT "$<MAKE_C_IDENTIFIER:${DEP_TARGET}-${ARG}>-type.txt"
            CONTENT
              "\
Dependent target:     ${DEP_TARGET}
Dependency:           ${ARG}
Dollar:               ${dollar}
Dependency type:      $<IF:$<TARGET_EXISTS:${ARG}>,$<GENEX_EVAL:${dependency_type}>,<not-a-target$<ANGLE-R>>
Dependency is module: ${gen_condition}
Link argument:        ${link_argument}
\
"
            )
          message(
            VERBOSE
            "
Extra information about possible late-defined-target dependency ${ARG} of target ${DEP_TARGET} will be written to:
${type_file}
at generation time. If ${ARG} is in fact a target, ensure it is defined before ${DEP_TARGET} to ensure better error checking and lower configuration overhead.\
"
            )
        endif()
        file(
          GENERATE
          OUTPUT "${error_file}"
          CONTENT
            "\
Target ${ARG} is of CMake type MODULE_LIBRARY, (as distinct from SHARED_LIBRARY). This means that it cannot be a library dependency, but can used only as a dynamically-loaded plugin module.

${ARG} was not defined at the time CMake processed instructions for dependent target ${DEP_TARGET}, so was not able to detect the error prior to ${DEP_TARGET}'s link operation. Please:

1. Define all targets *prior* to their use as dependencies to enable CMake to detect problems at configuration / generation time rather than at build time.

2. When generating plugins, ensure that the implementation code (allowing other code to access plugin functionality) is compiled separately and inserted into a different library from the plugin registration code (usually a CPP macro defined by your plugin framework). cetmodules' basic_plugin() CMake function will do this if instructed explicitly, or if the source code files follow the expected convention. See the documentation for basic_plugin() for details.

3. Use appropriate build system and/or linker mechanisms to ensure all dependencies for libraries and executables are resolved at link-time to avoid runtime failures which may be expensive in terms of wasted CPU cycles. With cetmodules, invoke set_compiler_flags(... NO_UNDEFINED ...).

4. Check the build tree for other *-ERROR.txt files and resolve them similarly prior to attempting another build.
\
"
          CONDITION "${gen_condition}"
          )
        if(scope STREQUAL PUBLIC)
          list(APPEND RESULT PRIVATE)
        endif()
        list(APPEND RESULT "${link_argument}")
      endif()
      if(scope STREQUAL PUBLIC)
        list(APPEND RESULT INTERFACE)
      endif()
      if(NOT scope STREQUAL PRIVATE)
        list(APPEND RESULT "${ARG}")
      endif()
      if(scope STREQUAL PUBLIC)
        list(APPEND RESULT ${scope})
      endif()
    endif()
  endif()
  set(${RESULT_VAR}
      "${RESULT}"
      PARENT_SCOPE
      )
endfunction()
