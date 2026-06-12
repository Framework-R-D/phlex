#ifndef PHLEX_CORE_MAKE_COMPUTATIONAL_EDGES_HPP
#define PHLEX_CORE_MAKE_COMPUTATIONAL_EDGES_HPP

// =========================================================================================
// make_computational_edges creates edges between nodes in the computational graph. It is
// used in framework_graph::finalize() after all user-facing nodes have been registered, and
// it is responsible for connecting producer nodes to consumer nodes through any necessary
// filter nodes. It also identifies which provider input ports are not connected to any
// producers, so that they can be directly connected to the source node.
//
// The function takes a node_catalog which contains all registered node types (producers,
// consumers, outputs, providers, etc.) and a filters map containing filter composite nodes
// created by make_filter_edges().
// =========================================================================================

#include "phlex/phlex_core_export.hpp"

#include "phlex/core/filter.hpp"
#include "phlex/core/index_router.hpp"
#include "phlex/core/node_catalog.hpp"

#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace phlex::experimental {

  PHLEX_CORE_EXPORT
  std::tuple<index_router::provider_input_ports_t, std::map<std::string, named_index_ports>>
  make_computational_edges(node_catalog& nodes, std::map<std::string, filter>& filters);

}

#endif // PHLEX_CORE_MAKE_COMPUTATIONAL_EDGES_HPP
