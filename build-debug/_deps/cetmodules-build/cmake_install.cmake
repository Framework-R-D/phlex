# Install script for directory: /workspace/build-debug/_deps/cetmodules-src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/opt/spack-environments/phlex-ci/.spack-env/view/bin/objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/workspace/build-debug/_deps/cetmodules-build/bin/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/workspace/build-debug/_deps/cetmodules-build/config/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/workspace/build-debug/_deps/cetmodules-build/etc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/workspace/build-debug/_deps/cetmodules-build/Modules/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/workspace/build-debug/_deps/cetmodules-build/libexec/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/workspace/build-debug/_deps/cetmodules-build/src/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/workspace/build-debug/_deps/cetmodules-build/test/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/cetmodules/cmake" TYPE FILE FILES "/workspace/build-debug/_deps/cetmodules-src/Modules/ParseVersionString.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  # Handle placeholders in target definitions.
  file(READ "/workspace/build-debug/_deps/cetmodules-build/genConfig/cetmodulesConfig.cmake" _targetFileData)
  string(REPLACE "@CET_DOLLAR@" "$" _targetFileData_new "${_targetFileData}")
  if (NOT _targetFileData_new STREQUAL _targetFileData)
    file(WRITE "/workspace/build-debug/_deps/cetmodules-build/genConfig/cetmodulesConfig.cmake" "${_targetFileData_new}")
  endif()
  if (NOT "" STREQUAL "x")
    # Check for unwanted absolute transitive dependencies.
    include("/workspace/build-debug/_deps/cetmodules-src/Modules/private/CetFindAbsoluteTransitiveDependencies.cmake")
    _cet_find_absolute_transitive_dependencies("/workspace/build-debug/_deps/cetmodules-build/genConfig/cetmodulesConfig.cmake"
       "${_targetFileData_new}"
       "")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/cetmodules/cmake" TYPE FILE FILES
    "/workspace/build-debug/_deps/cetmodules-build/genConfig/cetmodulesConfig.cmake"
    "/workspace/build-debug/_deps/cetmodules-build/cetmodulesConfigVersion.cmake"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/workspace/build-debug/_deps/cetmodules-build/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
