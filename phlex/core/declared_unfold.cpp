#include "phlex/core/declared_unfold.hpp"

#include "phlex/core/product_selector.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/model/products.hpp"
#include "phlex/utilities/hashing.hpp"

#include <gsl/pointers>
#include <oneapi/tbb/flow_graph.h>

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace phlex::detail {

  generator::generator(phlex::experimental::product_store_const_ptr const& parent,
                       gsl::not_null<phlex::experimental::algorithm_name const*> node_name,
                       gsl::not_null<phlex::experimental::identifier const*> stage,
                       std::string const& child_layer_name) :
    parent_{std::const_pointer_cast<phlex::experimental::product_store>(parent)},
    node_name_{node_name},
    stage_{stage},
    child_layer_name_{child_layer_name},
    child_layer_hash_{hash(parent->index()->layer_hash(),
                           phlex::experimental::identifier{child_layer_name_}.hash())}
  {
  }

  phlex::experimental::product_store_const_ptr generator::make_child(std::size_t const i,
                                                                     products new_products)
  {
    auto child_index = parent_->index()->make_child(child_layer_name_, i);
    ++child_count_;
    return std::make_shared<phlex::experimental::product_store>(
      child_index, node_name_, stage_, std::move(new_products));
  }

  declared_unfold::declared_unfold(phlex::experimental::algorithm_name name,
                                   std::vector<std::string> predicates,
                                   product_selectors input_products,
                                   tbb::flow::graph& graph,
                                   std::string child_layer) :
    products_consumer{std::move(name),
                      std::move(predicates),
                      std::move(input_products),
                      graph,
                      require_layers::always},
    child_layer_{std::move(child_layer)}
  {
  }

  declared_unfold::~declared_unfold() = default;
}
