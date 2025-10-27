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

  auto product_store::base() -> product_store_ptr { return product_store_ptr{new product_store}; }

  auto product_store::parent(std::string const& level_name) const noexcept -> product_store_const_ptr
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

  auto product_store::store_for_product(std::string const& product_name) const -> product_store_const_ptr
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

  auto product_store::make_flush() const -> product_store_ptr
  {
    return product_store_ptr{new product_store{parent_, id_, "[inserted]", stage::flush}};
  }

  auto product_store::make_continuation(std::string_view source,
                                                     products new_products) const -> product_store_ptr
  {
    return product_store_ptr{
      new product_store{parent_, id_, source, stage::process, std::move(new_products)}};
  }

  auto product_store::make_child(std::size_t new_level_number,
                                              std::string const& new_level_name,
                                              std::string_view source,
                                              products new_products) -> product_store_ptr
  {
    return product_store_ptr{new product_store{
      shared_from_this(), new_level_number, new_level_name, source, std::move(new_products)}};
  }

  auto product_store::make_child(std::size_t new_level_number,
                                              std::string const& new_level_name,
                                              std::string_view source,
                                              stage processing_stage) -> product_store_ptr
  {
    return product_store_ptr{new product_store{
      shared_from_this(), new_level_number, new_level_name, source, processing_stage}};
  }

  auto product_store::level_name() const noexcept -> std::string const& { return id_->level_name(); }
  auto product_store::source() const noexcept -> std::string_view { return source_; }
  auto product_store::parent() const noexcept -> product_store_const_ptr { return parent_; }
  auto product_store::id() const noexcept -> level_id_ptr const& { return id_; }
  auto product_store::is_flush() const noexcept -> bool { return stage_ == stage::flush; }

  auto product_store::contains_product(std::string const& product_name) const -> bool
  {
    return products_.contains(product_name);
  }

  auto more_derived(product_store_ptr const& a, product_store_ptr const& b) -> product_store_ptr const&
  {
    if (a->id()->depth() > b->id()->depth()) {
      return a;
    }
    return b;
  }
}
