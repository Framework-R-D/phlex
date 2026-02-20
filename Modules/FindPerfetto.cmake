# FindPerfetto.cmake
# Finds the Perfetto SDK (single-header implementation)

include(FindPackageHandleStandardArgs)

find_path(
  Perfetto_INCLUDE_DIR
  NAMES perfetto.h
  PATHS /opt/perfetto /usr/local/include /usr/include
  DOC "Perfetto SDK header location"
)

find_file(
  Perfetto_SOURCE
  NAMES perfetto.cc
  PATHS /opt/perfetto /usr/local/include /usr/include
  DOC "Perfetto SDK implementation file"
)

find_package_handle_standard_args(Perfetto REQUIRED_VARS Perfetto_INCLUDE_DIR Perfetto_SOURCE)

if(Perfetto_FOUND AND NOT TARGET Perfetto::Perfetto)
  find_package(Threads REQUIRED)
  add_library(perfetto_impl STATIC "${Perfetto_SOURCE}")
  target_include_directories(perfetto_impl PUBLIC "${Perfetto_INCLUDE_DIR}")
  target_compile_definitions(perfetto_impl PUBLIC PERFETTO_ENABLE_TRACING=1)
  target_link_libraries(perfetto_impl PUBLIC Threads::Threads)
  add_library(Perfetto::Perfetto ALIAS perfetto_impl)
endif()

mark_as_advanced(Perfetto_INCLUDE_DIR Perfetto_SOURCE)
