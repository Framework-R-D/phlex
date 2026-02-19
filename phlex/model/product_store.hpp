#ifndef PHLEX_MODEL_PRODUCT_STORE_HPP
#define PHLEX_MODEL_PRODUCT_STORE_HPP

#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/products.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>

namespace phlex::experimental {
  class product_store {
  public:
    explicit product_store(data_cell_index_ptr id,
                           std::string source = "Source",
                           products new_products = {});
    ~product_store();
    static product_store_ptr base(std::string base_name = "Source");

    auto begin() const noexcept { return products_.begin(); }
    auto end() const noexcept { return products_.end(); }
    auto size() const noexcept { return products_.size(); }
    bool empty() const noexcept { return products_.empty(); }

    std::string const& layer_name() const noexcept;
    std::string const& source() const noexcept;
    data_cell_index_ptr const& index() const noexcept;

    // Product interface
    bool contains_product(std::string const& key) const;

    template <typename T>
    T const& get_product(std::string const& key) const;

    template <typename T>
    handle<T> get_handle(std::string const& key) const;

    // Thread-unsafe operations
    template <typename T>
    void add_product(std::string const& key, T&& t);

    template <typename T>
    void add_product(std::string const& key, std::unique_ptr<product<T>>&& t);

  private:
    products products_{};
    data_cell_index_ptr id_;
    std::string
      source_; // FIXME: Should not have to copy the string (the source should outlive the product store)
  };

  product_store_ptr const& more_derived(product_store_ptr const& a, product_store_ptr const& b);

  // Non-template overload for single product_store_ptr case
  inline product_store_ptr const& most_derived(product_store_ptr const& store) { return store; }

  // Generic most_derived for tuples
  template <std::size_t I, typename Tuple>
  auto const& get_most_derived(Tuple const& tup, std::tuple_element_t<I - 1, Tuple> const& element)
  {
    constexpr auto N = std::tuple_size_v<Tuple>;
    if constexpr (I == N - 1) {
      return more_derived(element, std::get<I>(tup));
    } else {
      return get_most_derived<I + 1>(tup, more_derived(element, std::get<I>(tup)));
    }
  }

  template <typename T, typename U, typename... Ts>
  auto const& most_derived(std::tuple<T, U, Ts...> const& elements)
  {
    return get_most_derived<1ull>(elements, std::get<0>(elements));
  }

  // Implementation details
  template <typename T>
  void product_store::add_product(std::string const& key, T&& t)
  {
    add_product(key, std::make_unique<product<std::remove_cvref_t<T>>>(std::forward<T>(t)));
  }

  template <typename T>
  void product_store::add_product(std::string const& key, std::unique_ptr<product<T>>&& t)
  {
    products_.add(key, std::move(t));
  }

  template <typename T>
  [[nodiscard]] handle<T> product_store::get_handle(std::string const& key) const
  {
    return handle<T>{products_.get<T>(key), *id_};
  }

  template <typename T>
  [[nodiscard]] T const& product_store::get_product(std::string const& key) const
  {
    return *get_handle<T>(key);
  }
}

#endif // PHLEX_MODEL_PRODUCT_STORE_HPP
