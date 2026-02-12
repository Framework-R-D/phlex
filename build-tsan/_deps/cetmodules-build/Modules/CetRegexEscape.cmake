#[================================================================[.rst:
CetRegexEscape
--------------

Define the functions :command:`cet_regex_escape` and
:command:`cet_armor_string` to protect characters in strings from
unwanted interpolation or interpretation by CMake.

#]================================================================]

# Avoid unnecessary repeat inclusion.
include_guard()

# Non-disruptive CMake version requirements.
cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

#[================================================================[.rst:
.. command:: cet_armor_string

   Armor escaped characters against macro interpolation.

   .. code-block:: cmake

      cet_armor_string(<val> <var> [<num>])
      cet_armor_string(<val> ... VAR <var> [NUM <num>])

   Options
   ^^^^^^^

   ``[VAR ]<var>``
     Return the armored string in ``var``. Use of the keyword allows
     specification of a list of ``<val>`` rather than a single value.

   ``[NUM ]<num>``
     Armor against ``<num>`` levels of interpolation (default 1).

#]================================================================]

function(cet_armor_string)
  # Handle options.
  cmake_parse_arguments(PARSE_ARGV 0 CRE "" "VAR;NUM" "")
  _handle_regex_options(
    "\
USAGE: cet_armor_string(<val> <var> [<num>])
       cet_armor_string(<val>... VAR <var> [NUM <num>])"
    )
  if(DEFINED CRE_UNPARSED_ARGUMENTS)
    if(NOT DEFINED CRE_NUM)
      set(CRE_NUM 1) # Default.
    endif()
    # Duplicate any escape characters found to handle interpolation by CMake's
    # macro argument handling.
    if(CRE_NUM GREATER 0)
      foreach(count RANGE 1 ${CRE_NUM})
        string(REPLACE "\\" "\\\\" CRE_UNPARSED_ARGUMENTS
                       "${CRE_UNPARSED_ARGUMENTS}"
               )
      endforeach()
    endif()
    set(${CRE_VAR}
        "${CRE_UNPARSED_ARGUMENTS}"
        PARENT_SCOPE
        )
  else()
    unset(${CRE_VAR} PARENT_SCOPE)
  endif()
endfunction()

#[================================================================[.rst:
.. command:: cet_regex_escape

   Protect characters in a string or list of strings from interpretation
   as regex special characters by CMake.

   .. code-block:: cmake

      cet_regex_escape(<val> <var> [<num>])
      cet_regex_escape(<val> ... VAR <var> [NUM <num>])

   Options
   ^^^^^^^

   ``[VAR ]<var>``
     Return the protected string in ``var``. Use of the keyword allows
     specification of a list of ``<val>`` rather than a single value.

   ``[NUM ]<num>``
     Armor escaped characters against macro interpolation.

     .. seealso:: cet_armor_string.

#]================================================================]

function(cet_regex_escape)
  cmake_parse_arguments(PARSE_ARGV 0 CRE "" "VAR;NUM" "")
  # Handle options.
  _handle_regex_options(
    [=[
USAGE: cet_regex_escape(<val> <var> <num>)
       cet_regex_escape(<val>... VAR <var> [NUM <num>])]=]
    )
  # Escape special characters to prevent them being interpreted by the regex
  # engine.
  set(RESULT)
  foreach(val IN LISTS CRE_UNPARSED_ARGUMENTS)
    string(REGEX REPLACE [=[([].|^()?+$*\\[])]=] "\\\\\\1" val "${val}")
    string(REGEX REPLACE "/+" "/" val "${val}")
    # Armor against macro interpolation if requested via NUM.
    if(CRE_NUM GREATER 0)
      cet_armor_string("${val}" VAR val NUM ${CRE_NUM})
    endif()
    list(APPEND RESULT "${val}")
  endforeach()
  set(${CRE_VAR}
      "${RESULT}"
      PARENT_SCOPE
      )
endfunction()

function(_handle_regex_options MSG)
  if(NOT (DEFINED CRE_VAR OR DEFINED CRE_NUM)) # No keywords.
    list(LENGTH CRE_UNPARSED_ARGUMENTS len)
    if(len GREATER 3)
      message(FATAL_ERROR "${MSG}")
    endif()
    list(POP_FRONT CRE_UNPARSED_ARGUMENTS VAL CRE_VAR)
    set(CRE_UNPARSED_ARGUMENTS "${VAL}")
  endif()
  if(CRE_VAR)
    foreach(var IN ITEMS VAR NUM KEYWORDS_MISSING_VALUES UNPARSED_ARGUMENTS)
      if(DEFINED CRE_${var})
        set(CRE_${var}
            "${CRE_${var}}"
            PARENT_SCOPE
            )
      else()
        unset(CRE_${var} PARENT_SCOPE)
      endif()
    endforeach()
  else()
    message(FATAL_ERROR "${MSG}")
  endif()
endfunction()
