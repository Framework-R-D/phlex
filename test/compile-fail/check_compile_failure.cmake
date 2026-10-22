execute_process(
  COMMAND
    "${CMAKE_COMMAND}" --build "${BUILD_DIR}" --config "${BUILD_CONFIG}" --target "${BUILD_TARGET}"
  RESULT_VARIABLE result
  OUTPUT_VARIABLE output
  ERROR_VARIABLE error
)

if("${result}" STREQUAL "0")
  message(FATAL_ERROR "${BUILD_TARGET} compiled successfully but was expected to fail.")
endif()

string(FIND "${output}\n${error}" "${EXPECTED_DIAGNOSTIC}" diagnostic_position)
if(diagnostic_position EQUAL -1)
  message(
    FATAL_ERROR
    "${BUILD_TARGET} failed without the expected diagnostic: ${EXPECTED_DIAGNOSTIC}\n${output}\n${error}"
  )
endif()

message(STATUS "${BUILD_TARGET} failed with the expected diagnostic: ${EXPECTED_DIAGNOSTIC}")
