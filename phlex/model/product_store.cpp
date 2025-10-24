#include "phlex/model/product_store.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/products.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace phlex::experimental {

  product_store::product_store(product_store_const_ptr parent,
                               level_id_ptr id,
                               std::string_view source,
                               stage processing_stage,
                               products new_products) :
    parent_{std::move(parent)},
    products_{std::move(new_products)},
    id_{std::move(id)},
    source_{source},
    stage_{processing_stage}
  {
  }

  product_store::product_store(product_store_const_ptr parent,
                               std::size_t new_level_number,
                               std::string const& new_level_name,
                               std::string_view source,
                               products new_products) :
    parent_{parent},
    products_{std::move(new_products)},
    id_{parent->id()->make_child(new_level_number, new_level_name)},
    source_{source},
    stage_{stage::process}
  {
  }

  product_store::product_store(product_store_const_ptr parent,
                               std::size_t new_level_number,
                               std::string const& new_level_name,
                               std::string_view source,
                               stage processing_stage) :
    parent_{parent},
    id_{parent->id()->make_child(new_level_number, new_level_name)},
    source_{source},
    stage_{processing_stage}
  {
  }

  product_store::~product_store() = default;

  product_store_ptr product_store::base() { return product_store_ptr{new product_store}; }

  product_store_const_ptr product_store::parent(std::string const& level_name) const noexcept
  {
    auto store = parent_;
    while (store != nullptr) {
      if (store->level_name() == level_name) {
        return store;
      }
      store = store->parent_;
    }
    return nullptr;
  }

  product_store_const_ptr product_store::store_for_product(std::string const& product_name) const
  {
    auto store = shared_from_this();
    while (store != nullptr) {
      if (store->contains_product(product_name)) {
        return store;
      }
      store = store->parent_;
    }
    return nullptr;
  }

  product_store_ptr product_store::make_flush() const
  {
    return product_store_ptr{new product_store{parent_, id_, "[inserted]", stage::flush}};
  }

  product_store_ptr product_store::make_continuation(std::string_view source,
                                                     products new_products) const
  {
    return product_store_ptr{
      new product_store{parent_, id_, source, stage::process, std::move(new_products)}};
  }

  product_store_ptr product_store::make_child(std::size_t new_level_number,
                                              std::string const& new_level_name,
                                              std::string_view source,
                                              products new_products)
  {
    return product_store_ptr{new product_store{
      shared_from_this(), new_level_number, new_level_name, source, std::move(new_products)}};
  }

  product_store_ptr product_store::make_child(std::size_t new_level_number,
                                              std::string const& new_level_name,
                                              std::string_view source,
                                              stage processing_stage)
  {
    return product_store_ptr{new product_store{
      shared_from_this(), new_level_number, new_level_name, source, processing_stage}};
  }

  std::string const& product_store::level_name() const noexcept { return id_->level_name(); }
  std::string_view product_store::source() const noexcept { return source_; }
  product_store_const_ptr product_store::parent() const noexcept { return parent_; }
  level_id_ptr const& product_store::id() const noexcept { return id_; }
  bool product_store::is_flush() const noexcept { return stage_ == stage::flush; }

  bool product_store::contains_product(std::string const& product_name) const
  {
    return products_.contains(product_name);
  }

  product_store_ptr const& more_derived(product_store_ptr const& a, product_store_ptr const& b)
  {
    if (a->id()->depth() > b->id()->depth()) {
      return a;
    }
    return b;
  }
}
