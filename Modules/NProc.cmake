# This CMake module returns the result of 'nproc' (or its equivalent on Apple).
# The newline is trimmed from the result using the `tr -d '\n'` command.

if(APPLE)
  execute_process(COMMAND sysctl -n hw.logicalcpu COMMAND tr -d '\n' OUTPUT_VARIABLE NPROC)
elseif(LINUX)
  execute_process(COMMAND nproc COMMAND tr -d '\n' OUTPUT_VARIABLE NPROC)
endif()
