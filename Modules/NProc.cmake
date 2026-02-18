# This CMake module returns the result of 'nproc' (or its equivalent on Apple).
# The newline is trimmed from the result using the `tr -d '\n'` command.

if(APPLE)
  execute_process(
    COMMAND sysctl -n hw.logicalcpu
    OUTPUT_VARIABLE NPROC
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
elseif(LINUX)
  execute_process(COMMAND nproc OUTPUT_VARIABLE NPROC OUTPUT_STRIP_TRAILING_WHITESPACE)
endif()
