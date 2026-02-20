cet_make_library(
  LIBRARY_NAME
  phlex_graph
  SHARED
  SOURCE
  serializer_node.cpp
  LIBRARIES
  PRIVATE
  Boost::boost
  TBB::tbb
  phlex::utilities
  fmt::fmt
)
phlex_apply_symbol_visibility(phlex_graph)

install(FILES serial_node.hpp serializer_node.hpp DESTINATION include/phlex/graph)

target_include_directories(phlex_graph PRIVATE ${PROJECT_SOURCE_DIR})

# Interface library
cet_make_library(
  LIBRARY_NAME
  phlex_graph_int
  EXPORT_NAME
  graph
  INTERFACE
  NO_SOURCE
  LIBRARIES
  phlex_graph
  TBB::tbb
)
