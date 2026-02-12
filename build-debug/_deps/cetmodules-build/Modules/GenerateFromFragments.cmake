#[================================================================[.rst:
X
-
#]================================================================]
include_guard()

cmake_minimum_required(VERSION 3.18.2...4.1 FATAL_ERROR)

include(CetCMakeUtils)
include(CetPackagePath)

function(generate_from_fragments OUTFILE)
  # Assemble the OUTFILE from its components, configuring each as many times as
  # necessary to resolve all @VAR@ references, including those escaped with
  # @AT@. If OUTFILE ends in .in, the last configuration step will be omitted.
  get_filename_component(last_ext "${OUTFILE}" LAST_EXT)
  cmake_parse_arguments(
    PARSE_ARGV 1 GFF "NO_FRAGMENT_DELIMITERS" "" "FRAGMENTS"
    )
  list(APPEND GFF_FRAGMENTS ${GFF_UNPARSED_ARGUMENTS})
  if(last_ext STREQUAL .in) # Omit final configuration step.
    set(is_indirect "PARENT_")
    string(APPEND is_indirect "SCOPE") # Have to do this indirectly.
    set(GENFILE "${OUTFILE}")
  else()
    unset(is_indirect)
    set(GENFILE "${OUTFILE}.in")
  endif()
  set(used_fragments)
  set(file_op WRITE)
  set(pre)
  set(post)
  # Expand each fragment and add it to GENFILE.
  foreach(frag IN LISTS GFF_FRAGMENTS)
    get_filename_component(fragment "${frag}" ABSOLUTE)
    if(EXISTS "${fragment}")
      # Abbreviate if the path refers to this package.
      cet_package_path(frag_name PATH "${fragment}" MUST_EXIST)
      if(NOT frag_name)
        set(frag_name "${frag}")
      endif()
      _read_and_expand("${fragment}" "${frag_name}" frag_content)
      list(APPEND used_fragments "${frag_name}")
      if(frag_content)
        # We manage inter-file spacing carefully to avoid leading or trailing
        # whitespace in the file and ensure that empty frag_content does not
        # progress the file pointer.
        if(post)
          set(pre "${post}")
          unset(post)
        endif()
        if(frag_content MATCHES "^([\n \t]*)(.*[^\n \t])([\n \t]*)$")
          string(APPEND pre "${CMAKE_MATCH_1}")
          set(post "${CMAKE_MATCH_3}")
          set(frag_content "${CMAKE_MATCH_2}")
        endif()
        file(${file_op} "${GENFILE}" "${pre}${frag_content}")
        unset(pre)
        set(file_op APPEND)
      endif()
    else()
      set(msg "could not find specified fragment ${fragment}")
      if(NOT frag STREQUAL fragment)
        string(
          APPEND
          msg
          " - need absolute path rather than relative to \${CMAKE_CURRENT_SOURCE_DIR}?"
          )
      endif()
      message(FATAL_ERROR ${msg})
    endif()
  endforeach()
  if(file_op STREQUAL "APPEND") # We wrote something, at least.
    # Add a single newline to finish the file.
    file(${file_op} "${GENFILE}" "\n")
  endif()
  # ############################################################################
  # Generation metadata.
  if(used_fragments)
    set(FRAGMENTS_REPORT "# Compiled from:\n")
    foreach(frag IN LISTS used_fragments)
      string(APPEND FRAGMENTS_REPORT "#   ${frag}\n")
    endforeach()
  endif()
  cet_timestamp(GEN_TIME)
  # ############################################################################
  # Optional final configuration step, or pass GEN_TIME upstream for later use.
  if(is_indirect)
    # Need to be seen by parent.
    set(FRAGMENTS_REPORT "${FRAGMENTS_REPORT}" ${is_indirect})
    set(GEN_TIME "${GEN_TIME}" ${is_indirect})
  else()
    # Final configuration step.
    configure_file("${GENFILE}" "${OUTFILE}" @ONLY)
  endif()
  # ############################################################################
endfunction()

# Read and expand a config fragment as many times as necessary.
function(_read_and_expand FRAG FRAG_NAME RESULT_VAR)
  set(AT @) # For multi-pass variable expansion.
  file(READ "${FRAG}" FILE_IN)
  # Multi-pass variable expansion.
  set(pass_count 1)
  while(FILE_IN MATCHES "@AT@")
    string(CONFIGURE "${FILE_IN}" FILE_IN @ONLY)
    math(EXPR pass_count "${pass_count} + 1")
  endwhile()
  if(pass_count GREATER 1)
    set(pass_msg " (${pass_count} passes)")
  else()
    set(pass_msg)
  endif()
  if(GFF_NO_FRAGMENT_DELIMITERS)
    set(${RESULT_VAR}
        "${FILE_IN}"
        PARENT_SCOPE
        )
  else()
    # Wrap file content.
    string(JOIN "\n" result "###INCLUDE_BEGIN### ${FRAG_NAME}${pass_msg}"
           ${FILE_IN} "###INCLUDE_END###   ${FRAG_NAME}"
           )
    set(${RESULT_VAR}
        "${result}\n\n"
        PARENT_SCOPE
        )
  endif()
endfunction()
