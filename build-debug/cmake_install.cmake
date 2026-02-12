# Install script for directory: /workspace

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
  include("/workspace/build-debug/phlex/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/workspace/build-debug/plugins/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/workspace/build-debug/test/cmake_install.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  # Handle placeholders in target definitions.
  file(READ "/workspace/build-debug/CMakeFiles/Export/5716e694443994df33443dea2fa12c3e/phlexTargets.cmake" _targetFileData)
  string(REPLACE "@CET_DOLLAR@" "$" _targetFileData_new "${_targetFileData}")
  if (NOT _targetFileData_new STREQUAL _targetFileData)
    file(WRITE "/workspace/build-debug/CMakeFiles/Export/5716e694443994df33443dea2fa12c3e/phlexTargets.cmake" "${_targetFileData_new}")
  endif()
  if (NOT "" STREQUAL "x")
    # Check for unwanted absolute transitive dependencies.
    include("/workspace/build-debug/_deps/cetmodules-src/Modules/private/CetFindAbsoluteTransitiveDependencies.cmake")
    _cet_find_absolute_transitive_dependencies("/workspace/build-debug/CMakeFiles/Export/5716e694443994df33443dea2fa12c3e/phlexTargets.cmake"
       "${_targetFileData_new}"
       "")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/phlex/cmake/phlexTargets.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/phlex/cmake/phlexTargets.cmake"
         "/workspace/build-debug/CMakeFiles/Export/5716e694443994df33443dea2fa12c3e/phlexTargets.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/phlex/cmake/phlexTargets-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/phlex/cmake/phlexTargets.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/phlex/cmake" TYPE FILE FILES "/workspace/build-debug/CMakeFiles/Export/5716e694443994df33443dea2fa12c3e/phlexTargets.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/phlex/cmake" TYPE FILE FILES "/workspace/build-debug/CMakeFiles/Export/5716e694443994df33443dea2fa12c3e/phlexTargets-debug.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  # Handle placeholders in target definitions.
  file(READ "/workspace/build-debug/genConfig/phlexConfig.cmake" _targetFileData)
  string(REPLACE "@CET_DOLLAR@" "$" _targetFileData_new "${_targetFileData}")
  if (NOT _targetFileData_new STREQUAL _targetFileData)
    file(WRITE "/workspace/build-debug/genConfig/phlexConfig.cmake" "${_targetFileData_new}")
  endif()
  if (NOT "" STREQUAL "x")
    # Check for unwanted absolute transitive dependencies.
    include("/workspace/build-debug/_deps/cetmodules-src/Modules/private/CetFindAbsoluteTransitiveDependencies.cmake")
    _cet_find_absolute_transitive_dependencies("/workspace/build-debug/genConfig/phlexConfig.cmake"
       "${_targetFileData_new}"
       "")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/phlex/cmake" TYPE FILE FILES
    "/workspace/build-debug/genConfig/phlexConfig.cmake"
    "/workspace/build-debug/phlexConfigVersion.cmake"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/workspace/build-debug/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/workspace/build-debug/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
