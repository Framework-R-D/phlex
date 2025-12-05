add_library(phlex_utilities SHARED hashing.cpp resource_usage.cpp)
target_sources(
  phlex_utilities
  PUBLIC FILE_SET
         HEADERS
         BASE_DIRS
         ${PROJECT_SOURCE_DIR}
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
  )
target_include_directories(phlex_utilities PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(
  phlex_utilities
  PRIVATE Boost::boost
  PUBLIC spdlog::spdlog
  )

# Interface library
add_library(phlex_utilities_int INTERFACE)
target_include_directories(
  phlex_utilities_int INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
                                "$<INSTALL_INTERFACE:include>"
  )
target_link_libraries(
  phlex_utilities_int INTERFACE phlex_utilities TBB::tbb spdlog::spdlog
  )

add_library(phlex::utilities ALIAS phlex_utilities_int)

install(
  TARGETS phlex_utilities phlex_utilities_int
  EXPORT phlex
  FILE_SET HEADERS
  )
