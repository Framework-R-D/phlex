#[================================================================[.rst:
CetCmdWrapper
-------------

Invoke an external command with optional completion timestamp and/or
deletion of specified outputs on command error.

Synopsis
^^^^^^^^

.. parsed-literal::

  :variable:`${CMAKE_COMMAND}
  <cmake-ref-current:variable:CMAKE_COMMAND>` -DCMD=<cmd> \
  -DCMD_(ARGS|DONE_STAMP|DELETE_ON_FAILURE)=<val>...
  -P<path-to-CetCmdWrapper.cmake>

Variables affecting Behavior
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``CMD``
  The command to be invoked.

``CMD_ARGS``
  A semicolon-separated list of arguments to be passed to the command.

``CMD_DONE_STAMP``
  A file to be touched in the event of a successful (zero exit status)
  execution of the command.

``CMD_DELETE_ON_FAILURE``
  The specified semicolon-separated list of paths will be recursively
  deleted upon non-zero exit from the command.

#]================================================================]
cmake_minimum_required(VERSION 3.20...4.1 FATAL_ERROR)

if(NOT CMD)
  message(FATAL_ERROR "vacuous command")
endif()
string(REPLACE " " ";" CMD_ARGS "${CMD_ARGS}")
execute_process(
  COMMAND ${CMD} ${CMD_ARGS} RESULT_VARIABLE cmd_status COMMAND_ECHO STDERR
  )

if(cmd_status EQUAL 0)
  if(CMD_DONE_STAMP)
    file(TOUCH "${CMD_DONE_STAMP}")
  endif()
else() # Failure.
  if(CMD_DONE_STAMP)
    file(REMOVE "${CMD_DONE_STAMP}")
  endif()
  if(CMD_DELETE_ON_FAILURE)
    file(REMOVE_RECURSE "${CMD_DELETE_ON_FAILURE}")
  endif()
  set(err_msg "command failed: ${CMD}")
  if(cmd_status MATCHES "^-?[0-9]+$")
    string(APPEND err_msg " with status ${cmd_status}")
  else()
    string(APPEND err_msg "${cmd_status}")
  endif()
  message(FATAL_ERROR "${err_msg}")
endif()
