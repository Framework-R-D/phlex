#ifndef PHLEX_CORE_NODE_CATALOG_HPP
#define PHLEX_CORE_NODE_CATALOG_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/core/declared_fold.hpp"
#include "phlex/core/declared_observer.hpp"
#include "phlex/core/declared_output.hpp"
#include "phlex/core/declared_predicate.hpp"
#include "phlex/core/declared_transform.hpp"
#include "phlex/core/declared_unfold.hpp"
#include "phlex/core/producer_catalog.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/core/provider_node.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/core/source.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include "boost/pfr.hpp"

#include <string>
#include <vector>

namespace phlex::experimental {
  // A node_catalog is the framework_graph's registry of all the algorithm nodes
  // that make up a Phlex data-processing application.  It holds every kind of
  // node the framework knows about (predicates, observers, outputs, folds,
  // unfolds, transforms, providers, and sources), grouped by type, and serves
  // as the single source of node information used to build and run the flow
  // graph.

  // The externally visible entry point for plugin modules, with the signature:
  //   extern "C" void create_module(
  //     phlex::experimental::module_graph_proxy<phlex::experimental::void_tag> m,
  //     phlex::configuration const& config)
  //
  // User plugins define this function via the PHLEX_REGISTER_ALGORITHMS macro,
  // which expands to the extern "C" entry point.  The body invokes module methods
  // like transform(), predicate(), fold(), unfold(), observe(), and output() on
  // the 'm' argument, which add nodes to this node_catalog.
  //
  // During application initialization, the framework loads each configured
  // plugin shared library (PHLEX_PLUGIN_PATH), resolves the create_module entry
  // point via Boost.DLL, and invokes it to populate this node_catalog before
  // building the flow graph.

  struct PHLEX_CORE_EXPORT node_catalog {
    // How nodes are added to the node_catalog
    // ---------------------------------------
    // The node_catalog is populated as a side effect of *registration*, i.e.
    // when user algorithms are declared to be part of the framework_graph.
    // There are three insertion paths, all of which ultimately insert a node
    // into one of the simple_ptr_map members:
    //
    //   1. Algorithm registration (transform, predicate, observe, fold,
    //      unfold, provide, output):  Each declaration creates a short-lived
    //      *_api builder holding a registrar bound to the appropriate map (see
    //      registrar_for()).  For all but output nodes, the registrar's
    //      destructor creates the node and inserts in at the end of the
    //      registration statement. For output nodes, an explicit call does
    //      this.
    //   2. Sources: glue::source() inserts directly into `sources`, bypassing
    //      the registrar.
    //   3. Implicit provider_node objects: make_computational_edges() creates
    //      and inserts into `providers` later, during graph finalization, to
    //      satisfy otherwise unrequited inputs.
    //
    // Module (plugin) loading time
    // ----------------------------
    // Most registrations originate from dynamically loaded *modules*.  At graph
    // construction time, load_module() (phlex/app/load_module.cpp) loads each
    // configured plugin shared library found on PHLEX_PLUGIN_PATH. For each
    // plugin, the `create_module` entry point is found (usually defined via the
    // macro PHLEX_REGISTER_ALGORITHMS).
    //
    // The framework then invokes `create_module`, passing a module_graph_proxy
    // that forwards fold/observe/predicate/transform/unfold/output calls into
    // this catalog.  The plugin's registration code therefore runs during
    // single-threaded graph construction and adds its nodes here via the paths
    // described above.

    // Note: The loaded libraries' factory functions are kept alive for the
    // lifetime of the job so the libraries are not unloaded out from under the
    // registered nodes.

    // Only the framework_graph for an application is intended to have a
    // node_catalog; it should not get copied or assigned, so we disable copying
    // and moving to prevent accidental use.
    node_catalog() = default;
    node_catalog(node_catalog const&) = delete;
    node_catalog& operator=(node_catalog const&) = delete;

    template <typename Ptr>
    auto registrar_for(std::vector<std::string>& errors)
    {
      return registrar{boost::pfr::get<simple_ptr_map<Ptr>>(*this), errors};
    }

    std::size_t execution_count(std::string const& node_name) const;
    std::vector<products_consumer*> consumers() const;
    producer_catalog producers() const;

    simple_ptr_map<declared_predicate_ptr> predicates{};
    simple_ptr_map<declared_observer_ptr> observers{};
    simple_ptr_map<declared_output_ptr> outputs{};
    simple_ptr_map<declared_fold_ptr> folds{};
    simple_ptr_map<declared_unfold_ptr> unfolds{};
    simple_ptr_map<declared_transform_ptr> transforms{};
    simple_ptr_map<provider_node_ptr> providers{};
    simple_ptr_map<source_ptr> sources{};
  };
}

#endif // PHLEX_CORE_NODE_CATALOG_HPP
