#include "phlex/core/declared_unfold.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/utilities/hashing.hpp"

#include "fmt/std.h"
#include "spdlog/spdlog.h"

namespace phlex::experimental {

  generator::generator(product_store_const_ptr const& parent,
                       algorithm_name node_name,
                       std::string const& child_layer_name) :
    parent_{std::const_pointer_cast<product_store>(parent)},
    node_name_{std::move(node_name)},
    child_layer_name_{child_layer_name},
    child_layer_hash_{hash(parent->index()->layer_hash(), identifier{child_layer_name_}.hash())}
  {
  }

  product_store_const_ptr generator::make_child(std::size_t const i, products new_products)
  {
    auto child_index = parent_->index()->make_child(child_layer_name_, i);
    ++child_counts_;
    return std::make_shared<product_store>(child_index, node_name_, std::move(new_products));
  }

  declared_unfold::declared_unfold(algorithm_name name,
                                   std::vector<std::string> predicates,
                                   product_queries input_products,
                                   std::string child_layer) :
    products_consumer{std::move(name), std::move(predicates), std::move(input_products)},
    child_layer_{std::move(child_layer)}
  {
  }

  declared_unfold::~declared_unfold() = default;
}
