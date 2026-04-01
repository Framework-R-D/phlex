#ifndef PHLEX_CORE_PRODUCT_REGISTRY_HPP
#define PHLEX_CORE_PRODUCT_REGISTRY_HPP

#include "phlex/core/product_query.hpp"
#include "phlex/model/product_specification.hpp"

#include <forward_list>
#include <map>
#include <set>

namespace phlex::experimental {
  namespace detail {
    struct full_product_spec {
      product_specification spec;
      identifier layer;
      identifier stage;

      identifier const& plugin() const { return spec.plugin(); }
      identifier const& algorithm() const { return spec.algorithm(); }
      identifier const& suffix() const { return spec.suffix(); }
      type_id type() const { return spec.type(); }
    };
  }
  class product_registry {
  public:
    // We just need a container with cheap insertion (somewhere) that doesn't
    // invalidate iterators on insertion. Nothing gets accessed through the container,
    // that all happens through a set of multimaps.
    using store_t = std::forward_list<detail::full_product_spec>;
    // NB: clang-tidy's suggestion to use std::less<> causes a compile error
    using map_t = std::multimap<std::reference_wrapper<identifier const>,
                                store_t::const_pointer,
                                std::less<identifier>>;
    using type_map_t = std::multimap<type_id, store_t::const_pointer>;
    using result_set_t = std::set<store_t::const_pointer>;

    void add_product(product_specification spec, identifier layer, identifier stage);

    // A failed lookup in the registry is an error (an input product hasn't been registered yet,
    // so we can't determine the layer for a calculated output)
    detail::full_product_spec const& lookup(product_query const& query) const;

  private:
    store_t store_;
    map_t plugin_map_;
    map_t algorithm_map_;
    map_t suffix_map_;
    map_t layer_map_;
    map_t stage_map_;
    type_map_t type_map_;
  };
}

#endif // PHLEX_CORE_PRODUCT_REGISTRY_HPP
