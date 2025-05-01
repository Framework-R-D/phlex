add_library(phlex_utilities SHARED hashing.cpp resource_usage.cpp)
target_include_directories(phlex_utilities PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(phlex_utilities PRIVATE Boost::boost PUBLIC spdlog::spdlog)

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

install(TARGETS phlex_utilities)
