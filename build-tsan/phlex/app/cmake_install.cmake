# Install script for directory: /workspace/phlex/app

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
    set(CMAKE_INSTALL_CONFIG_NAME "RelWithDebInfo")
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

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librun_phlex.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librun_phlex.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librun_phlex.so"
         RPATH "/opt/spack-environments/phlex-ci/.spack-env/._view/dxv4eajuc2qloelbiagnao3afumqpdp5/lib:/opt/spack-environments/phlex-ci/.spack-env/view/lib")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/workspace/build-tsan/librun_phlex.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librun_phlex.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librun_phlex.so")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librun_phlex.so"
         OLD_RPATH "/workspace/build-tsan:/opt/spack-environments/phlex-ci/.spack-env/._view/dxv4eajuc2qloelbiagnao3afumqpdp5/lib:/opt/spack-environments/phlex-ci/.spack-env/view/lib:"
         NEW_RPATH "/opt/spack-environments/phlex-ci/.spack-env/._view/dxv4eajuc2qloelbiagnao3afumqpdp5/lib:/opt/spack-environments/phlex-ci/.spack-env/view/lib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/opt/spack-environments/phlex-ci/.spack-env/view/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/librun_phlex.so")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/phlex/app" TYPE FILE FILES
    "/workspace/phlex/app/load_module.hpp"
    "/workspace/phlex/app/run.hpp"
    "/workspace/phlex/app/version.hpp"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/phlex" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/phlex")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/phlex"
         RPATH "\$ORIGIN/../lib:/opt/spack-environments/phlex-ci/.spack-env/._view/dxv4eajuc2qloelbiagnao3afumqpdp5/lib:/opt/spack-environments/phlex-ci/.spack-env/view/lib")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE EXECUTABLE FILES "/workspace/build-tsan/bin/phlex")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/phlex" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/phlex")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/phlex"
         OLD_RPATH "/opt/spack-environments/phlex-ci/.spack-env/._view/dxv4eajuc2qloelbiagnao3afumqpdp5/lib:/workspace/build-tsan:/opt/spack-environments/phlex-ci/.spack-env/view/lib:"
         NEW_RPATH "\$ORIGIN/../lib:/opt/spack-environments/phlex-ci/.spack-env/._view/dxv4eajuc2qloelbiagnao3afumqpdp5/lib:/opt/spack-environments/phlex-ci/.spack-env/view/lib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/opt/spack-environments/phlex-ci/.spack-env/view/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/phlex")
    endif()
  endif()
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/workspace/build-tsan/phlex/app/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
