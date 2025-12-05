add_library(phlex_graph SHARED serializer_node.cpp)
target_sources(
  phlex_graph
  PUBLIC FILE_SET
         HEADERS
         BASE_DIRS
         ${PROJECT_SOURCE_DIR}
         FILES
         serial_node.hpp
         serializer_node.hpp
  )
target_include_directories(phlex_graph PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(
  phlex_graph PRIVATE Boost::boost TBB::tbb phlex::utilities fmt::fmt
  )

# Interface library
add_library(phlex_graph_int INTERFACE)
target_include_directories(
  phlex_graph_int INTERFACE "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>"
                            "$<INSTALL_INTERFACE:include>"
  )
target_link_libraries(phlex_graph_int INTERFACE phlex_graph TBB::tbb)

add_library(phlex::graph ALIAS phlex_graph_int)

install(
  TARGETS phlex_graph phlex_graph_int
  EXPORT phlex
  FILE_SET HEADERS
  )
