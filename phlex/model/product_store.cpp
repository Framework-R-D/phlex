#include "phlex/model/product_store.hpp"
#include "phlex/model/data_cell_index.hpp"

#include <memory>
#include <utility>

namespace phlex::experimental {

  product_store::product_store(data_cell_index_ptr id, std::string source, products new_products) :
    products_{std::move(new_products)}, id_{std::move(id)}, source_{std::move(source)}
  {
  }

  product_store::~product_store() = default;

  product_store_ptr product_store::base(std::string base_name)
  {
    return product_store_ptr{new product_store{data_cell_index::base_ptr(), std::move(base_name)}};
  }

  std::string const& product_store::layer_name() const noexcept { return id_->layer_name(); }
  std::string const& product_store::source() const noexcept { return source_; }
  data_cell_index_ptr const& product_store::index() const noexcept { return id_; }

  bool product_store::contains_product(std::string const& product_name) const
  {
    return products_.contains(product_name);
  }

  product_store_ptr const& more_derived(product_store_ptr const& a, product_store_ptr const& b)
  {
    if (a->index()->depth() > b->index()->depth()) {
      return a;
    }
    return b;
  }
}
