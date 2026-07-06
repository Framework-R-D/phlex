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

#include <string>
#include <type_traits>
#include <vector>

namespace phlex::detail {
  // A node_catalog is the framework_graph's registry of all the algorithm nodes
  // that make up a Phlex data-processing application.  It holds every kind of
  // node the framework knows about (predicates, observers, outputs, folds,
  // unfolds, transforms, and providers), grouped by type, and serves
  // as the single source of node information used to build and run the flow
  // graph. It also holds the sources, which are factories for creating provider
  // nodes.

  // The externally visible entry point for plugin modules is create_module,
  // with the signature:
  //   extern "C" void create_module(
  //     phlex::detail::module_graph_proxy<phlex::detail::void_tag> m,
  //     phlex::configuration const& config)
  //
  // User plugins define this function via the PHLEX_REGISTER_ALGORITHMS macro,
  // which expands to the extern "C" entry point.  The body invokes module methods
  // like transform(), predicate(), fold(), unfold(), observe(), and output() on
  // the 'm' argument. These methods add nodes to this node_catalog.
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
    //      destructor creates the node and inserts it at the end of the
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
    // and assignment to prevent accidental use.
    node_catalog() = default;
    node_catalog(node_catalog const&) = delete;
    node_catalog(node_catalog&&) = delete;
    node_catalog& operator=(node_catalog const&) = delete;
    node_catalog& operator=(node_catalog&&) = delete;
    ~node_catalog() = default;

    template <typename Ptr>
    auto registrar_for(std::vector<std::string>& errors)
    {
      return registrar{ptr_map_for<Ptr>(), errors};
    }

    source_vector sources_for(std::vector<std::string> const& keys) const;

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

  private:
    template <typename>
    static constexpr bool unknown_ptr_type_v{false};

    template <typename Ptr>
    auto& ptr_map_for()
    {
      // Note: We can eventually revert to boost::pfr::get once we move beyond LLVM 20.
      if constexpr (std::is_same_v<Ptr, declared_predicate_ptr>) {
        return predicates;
      } else if constexpr (std::is_same_v<Ptr, declared_observer_ptr>) {
        return observers;
      } else if constexpr (std::is_same_v<Ptr, declared_output_ptr>) {
        return outputs;
      } else if constexpr (std::is_same_v<Ptr, declared_fold_ptr>) {
        return folds;
      } else if constexpr (std::is_same_v<Ptr, declared_unfold_ptr>) {
        return unfolds;
      } else if constexpr (std::is_same_v<Ptr, declared_transform_ptr>) {
        return transforms;
      } else if constexpr (std::is_same_v<Ptr, provider_node_ptr>) {
        return providers;
      } else if constexpr (std::is_same_v<Ptr, source_ptr>) {
        return sources;
      } else {
        static_assert(unknown_ptr_type_v<Ptr>, "Unsupported node pointer type");
      }
    }
  };
}

#endif // PHLEX_CORE_NODE_CATALOG_HPP
