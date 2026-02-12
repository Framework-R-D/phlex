include_guard(DIRECTORY)

# Ensure that leading zeros are honored in project(VERSION ...) calls.
if (POLICY CMP0096)
  cmake_policy(SET CMP0096 NEW)
endif()

cmake_policy(PUSH)
cmake_minimum_required(VERSION 3.18.2...3.27 FATAL_ERROR)
include(private/CetOverrideFindPackage)
find_package( 4.01.01 REQUIRED)
cmake_policy(POP)
