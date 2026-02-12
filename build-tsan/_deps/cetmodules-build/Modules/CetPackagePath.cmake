#[================================================================[.rst:
CetPackagePath
--------------

Defines the function :command:`cet_package_path` to calculate a path
relative to a project's top level directory.

#]================================================================]

# Avoid unnecessary repeat inclusion.
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

#[================================================================[.rst:
.. command:: cet_package_path

   Calculate a path relative to the top-level directory of a project.

   .. code-block:: cmake

      cet_package_path(<out-var> [<option>] ...)

   Options
   ^^^^^^^

   ``BASE_SUBDIR <base-subdir>``
     Calculate the path relative to ``<proj-top>/<base-subdir>``;
     ``<base-subdir>`` must be a relative path.

   ``BINARY``
     Look for the path in the project's build tree (default: both source
     and build trees).

   ``FOUND_VAR <var>``
     Return an indication of the path's location: ``SOURCE`` (found in
     the project's source tree), ``BINARY`` (found in the project's
     binary tree) or ``NOTFOUND``.

   ``HUMAN_READABLE``
     ``<out-var>`` will contain a human-readable represtation of the
     calculated relative path.

   ``MUST_EXIST``
     If the path does not exist ``<out-var>`` will be set to
     ``NOTFOUND``.

   ``PATH <path>``
     The path for which the relative location should be calculated. If
     not specified, default to the current source or binary directory,
     as appropriate. ``<path>`` may be an absolute or relative path.

   ``SOURCE``
     Look for the path in the project's source tree (default: both
     source and build trees).

   ``SUBDIR <source-subdir>``
     .. deprecated:: 2.10.00 use ``PATH <source-subdir> SOURCE``

   ``TOP_PROJECT``
     Use the top-level—as opposed to the current—project in order to
     calculate the relative path.

   Non-option arguments
   ^^^^^^^^^^^^^^^^^^^^

   ``<out-var>``
     The name of a variable in which to return the calculated path.

   Details
   ^^^^^^^

   .. rst-class:: text-start

   Calculate the path to ``<path>`` (or
   ``CMAKE_CURRENT_(SOURCE|BINARY)_DIR``) relative to
   ``(PROJECT|CMAKE)_(SOURCE|BINARY)_DIR[/<base-subdir>]`` and save the
   result in ``<out-var>``.

   Specifying both ``SOURCE`` and ``BINARY`` is equivalent to specifying
   neither.

#]================================================================]

function(cet_package_path RESULT_VAR)
  cmake_parse_arguments(
    PARSE_ARGV 1 CPP "BINARY;HUMAN_READABLE;MUST_EXIST;SOURCE;TOP_PROJECT"
    "BASE_SUBDIR;FOUND_VAR;SUBDIR;PATH" ""
    )
  if(CPP_TOP_PROJECT)
    set(var_prefix CMAKE)
  else()
    set(var_prefix PROJECT)
  endif()
  if(CPP_SUBDIR) # Backward compatibility.
    if(NOT ARGC EQUAL 3)
      message(
        FATAL_ERROR
          "cet_package_path(): SUBDIR option is for backward compatibility ONLY: use PATH instead"
        )
    else()
      set(CPP_PATH "${CPP_SUBDIR}")
      set(CPP_SOURCE TRUE)
    endif()
  endif()
  if(NOT (CPP_SOURCE OR CPP_BINARY))
    set(CPP_SOURCE TRUE)
    set(CPP_BINARY TRUE)
  endif()
  if(CPP_SOURCE)
    _cpp_package_path(RESULT "${${var_prefix}_SOURCE_DIR}")
    if(RESULT)
      set(found SOURCE)
    endif()
  endif()
  if(CPP_BINARY AND NOT RESULT)
    _cpp_package_path(
      RESULT "${${var_prefix}_BINARY_DIR}" PATH_BASE
      "${CMAKE_CURRENT_BINARY_DIR}"
      )
    if(RESULT)
      set(found BINARY)
    else()
      set(found NOTFOUND)
    endif()
  endif()
  if(CPP_HUMAN_READABLE)
    if(RESULT STREQUAL .)
      if(BASE_SUBDIR)
        set(RESULT "<base>")
      else()
        set(RESULT "<top>")
      endif()
    endif()
  endif()
  set(${RESULT_VAR}
      "${RESULT}"
      PARENT_SCOPE
      )
  if(CPP_FOUND_VAR)
    set(${CPP_FOUND_VAR}
        ${found}
        PARENT_SCOPE
        )
  endif()
endfunction()

# Internal function to be called from cet_package_path ONLY.
function(_cpp_package_path VAR PROJ_BASE)
  cmake_parse_arguments(PARSE_ARGV 2 _cpp "" "PATH_BASE" "")
  get_filename_component(PUT "${CPP_PATH}" ABSOLUTE BASE_DIR ${_cpp_PATH_BASE})
  file(RELATIVE_PATH RESULT "${PROJ_BASE}/${CPP_BASE_SUBDIR}" "${PUT}")
  if(NOT RESULT) # Exact match.
    set(RESULT .)
  elseif(RESULT MATCHES [[^\.\./]]) # Not under expected base.
    set(RESULT)
  elseif(CPP_MUST_EXIST AND NOT EXISTS "${PUT}")
    set(RESULT NOTFOUND)
  endif()
  set(${VAR}
      "${RESULT}"
      PARENT_SCOPE
      )
endfunction()
