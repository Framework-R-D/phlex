cet_make_library(
  LIBRARY_NAME
  phlex_utilities
  SHARED
  SOURCE
  hashing.cpp
  resource_usage.cpp
  LIBRARIES
  PRIVATE
  Boost::boost
  PUBLIC
  spdlog::spdlog
)

install(
  FILES
    async_driver.hpp
    hashing.hpp
    max_allowed_parallelism.hpp
    resource_usage.hpp
    simple_ptr_map.hpp
    sized_tuple.hpp
    sleep_for.hpp
    string_literal.hpp
    thread_counter.hpp
  DESTINATION include/phlex/utilities
)

target_include_directories(phlex_utilities PRIVATE ${PROJECT_SOURCE_DIR})

# Interface library
cet_make_library(
  LIBRARY_NAME
  phlex_utilities_int
  EXPORT_NAME
  utilities
  INTERFACE
  NO_SOURCE
  LIBRARIES
  phlex_utilities
  TBB::tbb
  spdlog::spdlog
)
