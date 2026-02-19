#include "phlex/core/declared_unfold.hpp"
#include "phlex/model/data_cell_counter.hpp"
#include "phlex/model/handle.hpp"

#include "fmt/std.h"
#include "spdlog/spdlog.h"

namespace phlex::experimental {

  generator::generator(product_store_const_ptr const& parent,
                       std::string node_name,
                       std::string const& child_layer_name) :
    parent_{std::const_pointer_cast<product_store>(parent)},
    node_name_{std::move(node_name)},
    child_layer_name_{child_layer_name}
  {
  }

  product_store_const_ptr generator::make_child(std::size_t const i, products new_products)
  {
    auto child_index = parent_->index()->make_child(i, child_layer_name_);
    ++child_counts_[child_index->layer_hash()];
    return std::make_shared<product_store>(child_index, node_name_, std::move(new_products));
  }

  flush_counts_ptr generator::flush_result() const
  {
    if (not child_counts_.empty()) {
      return std::make_shared<flush_counts const>(std::move(child_counts_));
    }
    return nullptr;
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
