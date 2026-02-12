# Install script for directory: /workspace/phlex/core

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
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libphlex_core.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libphlex_core.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libphlex_core.so"
         RPATH "/opt/spack-environments/phlex-ci/.spack-env/._view/dxv4eajuc2qloelbiagnao3afumqpdp5/lib:/opt/spack-environments/phlex-ci/.spack-env/view/lib")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/workspace/build-tsan/libphlex_core.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libphlex_core.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libphlex_core.so")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libphlex_core.so"
         OLD_RPATH "/workspace/build-tsan:/opt/spack-environments/phlex-ci/.spack-env/._view/dxv4eajuc2qloelbiagnao3afumqpdp5/lib:/opt/spack-environments/phlex-ci/.spack-env/view/lib:"
         NEW_RPATH "/opt/spack-environments/phlex-ci/.spack-env/._view/dxv4eajuc2qloelbiagnao3afumqpdp5/lib:/opt/spack-environments/phlex-ci/.spack-env/view/lib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/opt/spack-environments/phlex-ci/.spack-env/view/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libphlex_core.so")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/phlex/core" TYPE FILE FILES
    "/workspace/phlex/core/concepts.hpp"
    "/workspace/phlex/core/consumer.hpp"
    "/workspace/phlex/core/declared_fold.hpp"
    "/workspace/phlex/core/declared_observer.hpp"
    "/workspace/phlex/core/declared_output.hpp"
    "/workspace/phlex/core/declared_predicate.hpp"
    "/workspace/phlex/core/declared_provider.hpp"
    "/workspace/phlex/core/declared_transform.hpp"
    "/workspace/phlex/core/declared_unfold.hpp"
    "/workspace/phlex/core/edge_creation_policy.hpp"
    "/workspace/phlex/core/edge_maker.hpp"
    "/workspace/phlex/core/filter.hpp"
    "/workspace/phlex/core/framework_graph.hpp"
    "/workspace/phlex/core/fwd.hpp"
    "/workspace/phlex/core/glue.hpp"
    "/workspace/phlex/core/graph_proxy.hpp"
    "/workspace/phlex/core/input_arguments.hpp"
    "/workspace/phlex/core/message.hpp"
    "/workspace/phlex/core/message_sender.hpp"
    "/workspace/phlex/core/multiplexer.hpp"
    "/workspace/phlex/core/node_catalog.hpp"
    "/workspace/phlex/core/product_query.hpp"
    "/workspace/phlex/core/products_consumer.hpp"
    "/workspace/phlex/core/registrar.hpp"
    "/workspace/phlex/core/registration_api.hpp"
    "/workspace/phlex/core/store_counters.hpp"
    "/workspace/phlex/core/upstream_predicates.hpp"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/phlex/core/fold" TYPE FILE FILES "/workspace/phlex/core/fold/send.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/phlex/core/detail" TYPE FILE FILES
    "/workspace/phlex/core/detail/make_algorithm_name.hpp"
    "/workspace/phlex/core/detail/maybe_predicates.hpp"
    "/workspace/phlex/core/detail/filter_impl.hpp"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/workspace/build-tsan/phlex/core/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
