#[================================================================[.rst:
ParseVersionString
------------------

Defines the following functions related to handling possibly
non-CMake-compliant version strings:

:command:`cet_compare_versions`
  Compare versions semantically, taking account of Greek, pre-release,
  release candidate, and patch versions. Returns ``TRUE`` or ``FALSE``
  according to a user-provided predicate specification.

:command:`cet_version_cmp`
  Low-level "`spaceship operator
  <https://en.wikipedia.org/wiki/Three-way_comparison#Spaceship_operator>`_"
  comparison between two versions, returning ``-1``, ``0``, or ``1``.

:command:`parse_version_string`
  Parse and process a version string or list.

:command:`to_cmake_version`
  Convert a version string or list into a CMake-compliant version
  string.

:command:`to_dot_version`
  Convert a version string or list into a ``.``-delimited version
  string.

:command:`to_version_string`
  Convert a version string or list into a ``.``-delimited version string
  with ``-``-separated non-numeric extra component.

#]================================================================]

include_guard()

# Need list(POP_FRONT...).
cmake_minimum_required(VERSION 3.15...4.1 FATAL_ERROR)

set(CET_PARSE_VERSION_STRING_MIN_CETMODULES_VERSION 2.21.00)

#[================================================================[.rst:
.. command:: cet_compare_versions

   .. code-block:: cmake

      cet_compare_versions(<result-var> <version> <pred> <ref-version>)

   Compare ``<version>`` with ``<ref-version>`` semantically according
   to predicate ``<pred>``, respecting any trailing non-numeric version
   components ``<extra>``, and placing the answer in ``<result-var>``.

   Details
   ^^^^^^^

   ``<pred>`` is valid if there exists a CMake :command:`if()
   <cmake-ref-current:command:if>` predicate ``VERSION_<pred>``.

   Comparison order for ``(<version>-?)?(<extra-text>(-?<extra-num>)?)?``:

   * ``<version>-?alpha(-?[0-9]+)?`` <
   * ``<version>-?beta(-?[0-9]+)?`` <
   * ``<version>-?gamma(-?[0-9]+)?`` <
   * ``<version>-?(rc|pre)(-?[0-9]+)?`` <
   * ``<version>-?`` <
   * ``<version>-?(patch-?[0-9]*|p-?[0-9]+)`` <
   * ``<version>-?.+`` <
   * ``<extra>``

   Non-numeric component prefixes such as ``alpha``, ``beta``, etc. are
   case-insensitive, and equivalent prefixes (``Alpha-1`` and ``a01``,
   say, or ``pre`` and ``rc``) will compare equal.

   A version string—after the stripping of a single leading ``v`` if
   present—beginning with a separator or non-numeric character will
   always compare greater than a similarly stripped version string
   beginning with a numeric component, and equal to every other such
   version string regardless of any numeric suffix.

   A non-numeric component with no numeric suffix will compare equal to
   an equivalent non-numeric component with a numeric suffix comparing
   equal to 0 numerically.

   Numeric version components—including any numeric suffix to a trailing
   non-numeric component—will always be compared numerically
   i.e. without regard to leading zeros.

#]================================================================]

function(cet_compare_versions _CMPV_RESULT_VAR _CMPV_VERSION _CMPV_PRED
         _CMPV_REF
         )
  string(TOUPPER "${_CMPV_PRED}" _CMPV_PRED)
  set(_cmpv_result FALSE)
  cet_version_cmp(_cmpv_cmp "${_CMPV_VERSION}" "${_CMPV_REF}")
  if(_CMPV_PRED MATCHES "^VERSION_LESS(_EQUAL)?$")
    if(_cmpv_cmp LESS 0 OR (CMAKE_MATCH_1 AND NOT _cmpv_result))
      set(_cmpv_result TRUE)
    endif()
  elseif(_CMPV_PRED MATCHES "^VERSION_GREATER(_EQUAL)?$")
    if(_cmpv_cmp GREATER 0 OR (CMAKE_MATCH_1 AND NOT _cmpv_result))
      set(_cmpv_result TRUE)
    endif()
  elseif(_CMPV_PRED STREQUAL "VERSION_EQUAL")
    if(NOT _cmpv_cmp)
      set(_cmpv_result TRUE)
    endif()
  else()
    message(FATAL_ERROR "predicate \"${_CMPV_PRED}\" not recognized")
  endif()
  set(${_CMPV_RESULT_VAR}
      ${_cmpv_result}
      PARENT_SCOPE
      )
endfunction()

#[================================================================[.rst:
.. command:: cet_version_cmp

   .. code-block:: cmake

      cet_compare_versions(<result-var> <version> <ref-version>)

   Low-level "`spaceship operator
   <https://en.wikipedia.org/wiki/Three-way_comparison#Spaceship_operator>`_"
   comparison between ``<version>`` and ``<ref-version>``, returning
   ``-1``, ``0`` or ``1`` in ``<result-var>`` based on whether
   ``<version>`` is semantically less than, equal to or greater than
   ``<ref-version>``.

   *cf*  :command:`cet_compare_versions`.

#]================================================================]

function(cet_version_cmp _CVC_RESULT_VAR _CVC_VERSION _CVC_REF)
  parse_version_string("${_CVC_VERSION}" _cvc_version_info)
  parse_version_string("${_CVC_REF}" _cvc_ref_info)
  set(_cvc_result 0)
  to_cmake_version(
    _cvc_version_info _cvc_version_short EXTRA_VAR _cvc_version_extra
    )
  to_cmake_version(_cvc_ref_info _cvc_ref_short EXTRA_VAR _cvc_ref_extra)
  if(NOT ("${_cvc_version_short}" STREQUAL "" OR "${_cvc_ref_short}" STREQUAL ""
         )
     )
    if(_cvc_version_short VERSION_LESS _cvc_ref_short)
      set(_cvc_result -1)
    elseif(_cvc_ref_short VERSION_LESS _cvc_version_short)
      set(_cvc_result 1)
    endif()
  endif()
  if(NOT _cvc_result)
    parse_version_string(
      _cvc_version_info
      _cvc_dummy
      _cvc_dummy
      _cvc_dummy
      _cvc_dummy
      _cvc_dummy
      _cvc_version_extra_type
      _cvc_version_extra_text
      _cvc_version_extra_num
      )
    if(NOT _cvc_version_extra_type)
      set(_cvc_version_extra_type 0)
    endif()
    parse_version_string(
      _cvc_ref_info
      _cvc_dummy
      _cvc_dummy
      _cvc_dummy
      _cvc_dummy
      _cvc_dummy
      _cvc_ref_extra_type
      _cvc_ref_extra_text
      _cvc_ref_extra_num
      )
    if(NOT _cvc_ref_extra_type)
      set(_cvc_ref_extra_type 0)
    endif()
    # Type codes are ordered.
    if(${_cvc_version_extra_type} GREATER ${_cvc_ref_extra_type})
      set(_cvc_result 1)
    elseif(${_cvc_ref_extra_type} GREATER ${_cvc_version_extra_type})
      set(_cvc_result -1)
    elseif(${_cvc_version_extra_type} EQUAL 2) # Non-special suffix.
      if("${_cvc_version_extra_text}" STRLESS "${_cvc_ref_extra_text}")
        set(_cvc_result -1)
      elseif("${_cvc_ref_extra_text}" STRLESS "${_cvc_version_extra_text}")
        set(_cvc_result 1)
      endif()
    elseif(${_cvc_version_extra_type} EQUAL 3 OR ${_cvc_version_extra_type}
                                                 GREATER 100
           )
      # Look for a timestamp-ish
      foreach(v IN ITEMS version ref)
        set(_cvc_${v}_date)
        set(_cvc_${v}_hh "00")
        set(_cvc_${v}_mm "00")
        set(_cvc_${v}_ss 0)
        if(_cvc_${v}_extra_num
           MATCHES
           "^([0-9][0-9][0-9][0-9][0-1][0-9][0-3][0-9])(([0-2][0-9])(([0-5][0-9])(([0-5][0-9])\\.?([0-9]+)?)?)?)?$"
           )
          set(_cvc_${v}_date "${CMAKE_MATCH_1}")
          if(NOT "${CMAKE_MATCH_3}" STREQUAL "")
            set(_cvc_${v}_hh "${CMAKE_MATCH_3}")
            if(NOT "${CMAKE_MATCH_5}" STREQUAL "")
              set(_cvc_${v}_mm "${CMAKE_MATCH_5}")
              if(NOT "${CMAKE_MATCH_7}" STREQUAL "")
                set(_cvc_${v}_ss "${CMAKE_MATCH_7}")
                if(NOT "${CMAKE_MATCH_8}" STREQUAL "")
                  string(APPEND _cvc_${v}_ss ".${CMAKE_MATCH_8}")
                endif()
              endif()
            endif()
          endif()
        endif()
      endforeach()
      if(_cvc_version_date)
        if(_cvc_version_date VERSION_LESS _cvc_ref_date)
          set(_cvc_result -1)
        elseif(_cvc_version_date VERSION_GREATER _cvc_ref_date)
          set(_cvc_result -1)
        elseif("${_cvc_version_hh}.${_cvc_version_mm}" VERSION_LESS
               "${_cvc_ref_hh}.${_cvc_ref_mm}"
               )
          set(_cvc_result -1)
        elseif("${_cvc_version_hh}.${_cvc_version_mm}" VERSION_GREATER
               "${_cvc_ref_hh}.${_cvc_ref_mm}"
               )
          set(_cvc_result 1)
        elseif(_cvc_version_ss LESS _cvc_ref_ss)
          set(_cvc_result -1)
        elseif(_cvc_version_ss GREATER _cvc_ref_ss)
          set(_cvc_result 1)
        endif()
      elseif(_cvc_ref_date)
        if(_cvc_version_date VERSION_GREATER _cvc_ref_date)
          set(_cvc_result 1)
        elseif(NOT _cvc_version_date VERSION_EQUAL _cvc_ref_date)
          set(_cvc_result -1)
        endif()
      endif()
    endif()
    if(NOT
       (_cvc_result
        OR _cvc_version_date
        OR _cvc_ref_date)
       )
      if("${_cvc_version_extra_num}" STREQUAL "")
        set(_cvc_version_extra_num 0)
      endif()
      if("${_cvc_ref_extra_num}" STREQUAL "")
        set(_cvc_ref_extra_num 0)
      endif()
      if(_cvc_version_extra_num LESS _cvc_ref_extra_num)
        set(_cvc_result -1)
      elseif(_cvc_ref_extra_num LESS _cvc_version_extra_num)
        set(_cvc_result 1)
      endif()
    endif()
  endif()
  set(${_CVC_RESULT_VAR}
      ${_cvc_result}
      PARENT_SCOPE
      )
endfunction()

#[================================================================[.rst:
.. command:: parse_version_string

   Version parsing and transformation.

   .. parsed-literal::

      parse_version_string([`VERSION`_] <version> [<options>] <var> [<var> ...])
      parse_version_string(`SEP`_ <output-sep> [VERSION] <version>  [<options>] <var>)

   Common Options
   ^^^^^^^^^^^^^^

   ``INPUT_PREAMBLE <pre>``
     ``<pre>`` will be removed from the beginning of ``<version>`` prior
     to parsing (if present). Default "v."

   ``[VERSION] <version>``
     The version to be parsed and/or transformed. ``<version>`` may be:

     #. A version string of the form:

        .. parsed-literal::

           [<pre>][<major>[<sep><minor>[<sep><patch>[<sep><tweak>]]]]
           [<extra-sep>][<extra>]

     #. A semi-colon-separated list of version components.

     #. The name of a CMake variable containing either of the above.

   Signatures
   ^^^^^^^^^^

   .. signature::
      parse_version_string(VERSION <version> ...)

      .. parsed-literal::

         parse_version_string(VERSION <version> [<options>] <var> [<var> ...])

      Parse a version string into its component parts to be returned in
      ``<var>``.

      Details
      ^^^^^^^

      If there is one ``<var>`` it will contain a list of all parsed
      components (``<major>``, ``<minor>``, ``<patch>``, ``<tweak>`` and
      ``<extra>``); for multiple, the components will be mapped to each
      ``<var>`` with un-placed components being discarded.

      ``<sep>`` may be ``-``, ``_``, or ``.``

      Per CMake convention: ``<major>``, ``<minor>``, ``<patch>``, and
      ``<tweak>`` must be non-negative integers (with optional leading
      zeros), so ``<extra>`` starts from the first non-separator,
      non-numeric character after any numeric version components have
      been identified.

   .. rst-class:: text-start

   .. signature::
      parse_version_string(SEP <output-sep> [VERSION] <version> ...)

      .. parsed-literal::

         parse_version_string(SEP <output-sep> [VERSION] <version> [<options>] <var>)

      Transform ``<version>`` into an ``<output-sep>``-delimited version
      string, returned in ``<var>``.

      Options
      ^^^^^^^

      ``EXTRA_SEP <output-extra-sep>``
        ``<extra>``\ —if present—will be separated from the rest of the
        version string by ``<output-extra-sep>``.

      ``NO_EXTRA``
        ``<extra>`` will be omitted from ``<var>``; mutually exclusive
        with ``EXTRA_SEP``.

      ``PREAMBLE <output-preamble>``
        ``<output-preamble>`` will be prepended to ``<var>``.

      Details
      ^^^^^^^

      Vacuous intermediate components will be represented by ``0`` in
      ``<var>``.

#]================================================================]

function(parse_version_string)
  # Argument parsing and validation.
  cmake_parse_arguments(
    PARSE_ARGV 0 PVS "NO_EXTRA"
    "EXTRA_VAR;INPUT_PREAMBLE;PREAMBLE;SEP;EXTRA_SEP;VERSION" ""
    )
  if(NOT PVS_VERSION)
    if(NOT PVS_UNPARSED_ARGUMENTS)
      message(FATAL_ERROR "vacuous VERSION not permitted")
    endif()
    list(POP_FRONT PVS_UNPARSED_ARGUMENTS PVS_VERSION)
  endif()
  list(POP_FRONT PVS_UNPARSED_ARGUMENTS _PVS_VAR)
  if(NOT _PVS_VAR)
    message(FATAL_ERROR "missing required non-option argument VAR")
  endif()
  if(NOT DEFINED PVS_INPUT_PREAMBLE)
    set(PVS_INPUT_PREAMBLE "v")
  endif()
  if(DEFINED PVS_SEP AND PVS_UNPARSED_ARGUMENTS)
    message(
      WARNING
        "parse_version_string(): ignoring unexpected extra"
        " non-option arguments ${PVS_UNPARSED_ARGUMENTS} when SEP specified"
      )
  endif()
  if(PVS_SEP STREQUAL "")
    if(DEFINED PVS_EXTRA_SEP)
      message(WARNING "EXTRA_SEP ignored without non-vacuous SEP")
      unset(PVS_EXTRA_SEP)
    endif()
    if(PVS_NO_EXTRA)
      message(WARNING "NO_EXTRA ignored without non-vacuous SEP")
      unset(PVS_NO_EXTRA)
    endif()
    if(PVS_EXTRA_VAR)
      message(WARNING "EXTRA_VAR ignored without non-vacuous SEP")
      unset(PVS_EXTRA_VAR)
    endif()
  elseif(PVS_NO_EXTRA AND PVS_EXTRA_SEP)
    message(FATAL_ERROR "NO_EXTRA and EXTRA_SEP are mutually exclusive")
  endif()
  # Initialize intermediate variables.
  unset(_pvs_extra)
  unset(_pvs_extra_bits)
  unset(_pvs_tmp_bits)
  set(_pvs_sep_def "[-_.]")
  if(${PVS_VERSION} MATCHES "^.+$") # Handle a level of indirection.
    set(PVS_VERSION "${CMAKE_MATCH_0}")
  else()
    set(PVS_VERSION "")
  endif()
  list(LENGTH PVS_VERSION _pvs_sz)
  if(_pvs_sz GREATER 1) # We have a pre-parsed list.
    if(_pvs_sz GREATER 4) # We have a trailing non-numeric version component.
      list(GET PVS_VERSION 4 _pvs_extra)
      if(_pvs_sz GREATER 5)
        list(SUBLIST PVS_VERSION 5 -1 _pvs_extra_bits)
      endif()
    endif()
    list(SUBLIST PVS_VERSION 0 4 _pvs_tmp_bits)
  elseif(NOT "${PVS_VERSION}" STREQUAL "")
    set(_pvs_failed TRUE)
    unset(_pvs_major)
    unset(_pvs_minor)
    unset(_pvs_patch)
    unset(_pvs_tweak)
    string(LENGTH "${PVS_INPUT_PREAMBLE}" ip_length)
    if(ip_length GREATER 0)
      string(FIND "${PVS_VERSION}" "${PVS_INPUT_PREAMBLE}" ip_idx)
      if(ip_idx EQUAL 0)
        string(SUBSTRING "${PVS_VERSION}" ${ip_length} -1 PVS_VERSION)
      endif()
    endif()
    if("${PVS_VERSION}" MATCHES "^([0-9]*)(${_pvs_sep_def}?)(.*)$")
      set(_pvs_major "${CMAKE_MATCH_1}")
      if(NOT "${CMAKE_MATCH_2}" STREQUAL "")
        set(_pvs_sep "[${CMAKE_MATCH_2}]")
      else()
        set(_pvs_sep "${_pvs_sep_def}")
      endif()
      if("${CMAKE_MATCH_3}" MATCHES "^([0-9]*)${_pvs_sep}?(.*)$")
        set(_pvs_minor "${CMAKE_MATCH_1}")
        if("${CMAKE_MATCH_2}" MATCHES "^([0-9]*)${_pvs_sep}?(.*)$")
          set(_pvs_patch "${CMAKE_MATCH_1}")
          # Allow a different separator for the non-numeric component.
          if("${CMAKE_MATCH_2}" MATCHES "^([0-9]*)(${_pvs_sep_def}?(.*))$")
            set(_pvs_tweak "${CMAKE_MATCH_1}")
            if(NOT "${CMAKE_MATCH_2}" STREQUAL "")
              set(_pvs_extra "${CMAKE_MATCH_3}")
            endif()
            set(_pvs_failed FALSE)
          endif()
        endif()
      endif()
    endif()
    if(_pvs_failed)
      message(
        FATAL_ERROR
          "parse_version_string() cannot parse a version from \"${PVS_VERSION}\""
        )
    else()
      # Make sure a non-empty component is placed at the correct place in the
      # _pvs_tmp_bits component list. Fill with "0<sep>" if we're generating a
      # string.
      foreach(_pvs_tmp_element IN ITEMS _pvs_tweak _pvs_patch _pvs_minor
                                        _pvs_major
              )
        # Important to go through the components in reverse.
        if(${_pvs_tmp_element} STREQUAL "")
          if("${_pvs_tmp_bits}" MATCHES "[^;]")
            if("${PVS_SEP}" STREQUAL "")
              # Need at least a placeholder while we're building the array.
              string(PREPEND _pvs_tmp_bits ";")
            else()
              list(PREPEND _pvs_tmp_bits 0)
            endif()
          endif()
        else()
          list(INSERT _pvs_tmp_bits 0 "${${_pvs_tmp_element}}")
        endif()
      endforeach()
    endif()
  endif()
  if(PVS_SEP)
    # Generating a string.
    string(JOIN "${PVS_SEP}" _pvs_tmp_string ${_pvs_tmp_bits})
    if(NOT PVS_NO_EXTRA)
      if(NOT
         ("${_pvs_tmp_string}" STREQUAL ""
          OR "${PVS_EXTRA_SEP}" STREQUAL ""
          OR "${_pvs_extra}" STREQUAL "")
         )
        string(APPEND _pvs_tmp_string "${PVS_EXTRA_SEP}")
      endif()
      string(APPEND _pvs_tmp_string ${_pvs_extra})
    endif()
    if(NOT "${PVS_EXTRA_VAR}" STREQUAL "")
      set(${PVS_EXTRA_VAR}
          ${_pvs_extra}
          PARENT_SCOPE
          )
    endif()
    if(NOT "${_pvs_tmp_string}" STREQUAL "")
      string(PREPEND _pvs_tmp_string ${PVS_PREAMBLE})
    endif()
    set(${_PVS_VAR}
        ${_pvs_tmp_string}
        PARENT_SCOPE
        )
  else()
    if(DEFINED _pvs_extra)
      # Make sure the bits array is padded appropriately.
      list(LENGTH _pvs_tmp_bits _pvs_sz)
      if(_pvs_sz EQUAL 0)
        set(_pvs_tmp_bits ";;;;${_pvs_extra}")
      else()
        math(EXPR _pvs_pad_sz "5 - ${_pvs_sz}")
        string(REPEAT ";" ${_pvs_pad_sz} _pvs_bits_pad)
        string(APPEND _pvs_tmp_bits "${_pvs_bits_pad}${_pvs_extra}")
        unset(_pvs_pad_sz)
      endif()
      if(NOT (_pvs_extra STREQUAL "" OR DEFINED _pvs_extra_bits))
        _cet_parse_version_extra()
      endif()
      list(APPEND _pvs_tmp_bits ${_pvs_extra_bits})
    endif()
    if(PVS_UNPARSED_ARGUMENTS)
      # Put each component in a variable.
      foreach(_pvs_v IN LISTS _PVS_VAR PVS_UNPARSED_ARGUMENTS)
        list(POP_FRONT _pvs_tmp_bits _pvs_tmp_element)
        set(${_pvs_v}
            ${_pvs_tmp_element}
            PARENT_SCOPE
            )
      endforeach()
    else()
      # Return the result as a list of components.
      set(${_PVS_VAR}
          "${_pvs_tmp_bits}"
          PARENT_SCOPE
          )
    endif()
  endif()
endfunction()

#[================================================================[.rst:
.. command:: to_cmake_version

   .. code-block:: cmake

      to_cmake_version(<version> <var> ...)

   Equivalent to:

   .. parsed-literal::

      :command:`parse_version_string <parse_version_string(sep)>`\ (<version> SEP . NO_EXTRA ...)

#]================================================================]

macro(to_cmake_version _TCV_VERSION _TCV_VAR)
  parse_version_string("${_TCV_VERSION}" SEP . NO_EXTRA ${_TCV_VAR} ${ARGN})
endmacro()

#[================================================================[.rst:
.. command:: to_dot_version

   .. code-block:: cmake

      to_dot_version(<version> <var> ...)

   Equivalent to:

   .. parsed-literal::

      :command:`parse_version_string <parse_version_string(sep)>`\ (<version> SEP . <var> ...)

#]================================================================]

macro(to_dot_version _TDV_VERSION _TDV_VAR)
  parse_version_string("${_TDV_VERSION}" SEP . ${_TDV_VAR} ${ARGN})
endmacro()

#[================================================================[.rst:
.. command:: to_version_string

   .. code-block:: cmake

      to_version_string(<version> <var> ...)

   Equivalent to:

   .. parsed-literal::

      :command:`parse_version_string <parse_version_string(sep)>`\ (<version> SEP . EXTRA_SEP - <var>...)

#]================================================================]

macro(to_version_string _TDV_VERSION _TDV_VAR)
  parse_version_string(
    "${_TDV_VERSION}"
    SEP
    .
    EXTRA_SEP
    -
    ${_TDV_VAR}
    ${ARGN}
    )
endmacro()

function(_cet_parse_version_extra)
  if("${_pvs_extra}" MATCHES "[0-9]+(\\.?[0-9]*)?$")
    set(_pe_num "${CMAKE_MATCH_0}")
    string(FIND "${_pvs_extra}" "${_pe_num}" _pe_num_idx REVERSE)
    string(SUBSTRING "${_pvs_extra}" 0 ${_pe_num_idx} _pe_text)
    if(_pe_text MATCHES "^(.*)[_.-]$") # Trim text-num separator.
      set(_pe_text "${CMAKE_MATCH_1}")
    endif()
  else()
    set(_pe_num)
    set(_pe_text "${_pvs_extra}")
  endif()
  string(TOLOWER "${_pe_text}" _pe_text_l)
  if(_pe_text STREQUAL "")
    set(_pe_type 0)
  elseif(_pe_text_l MATCHES "^(.+-)?(nightly|snapshot)$")
    if(_pvs_sz)
      set(_pe_type 3)
    else() # No numeric version component.
      set(_pe_type 103)
    endif()
  elseif(_pvs_sz EQUAL 0) # No leading numeric component at all.
    set(_pe_type 101)
    set(_pe_num)
    set(_pe_text "${_pvs_extra}")
  elseif(_pe_text_l STREQUAL "patch"
         OR (_pe_text_l STREQUAL "p" AND NOT _pe_num STREQUAL "")
         )
    set(_pe_type 1)
  elseif(_pe_text_l STREQUAL "rc" OR _pe_text_l STREQUAL "pre")
    set(_pe_type -1)
  elseif(_pe_text_l STREQUAL "gamma")
    set(_pe_type -2)
  elseif(_pe_text_l STREQUAL "beta")
    set(_pe_type -3)
  elseif(_pet_ext_l STREQUAL "alpha")
    set(_pe_type -4)
  else()
    set(_pe_type 2)
    set(_pe_text "${_pe_text_l}") # Standardize for comparison.
  endif()
  set(_pvs_extra_bits
      ${_pe_type} "${_pe_text}" ${_pe_num}
      PARENT_SCOPE
      )
endfunction()
