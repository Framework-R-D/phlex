#ifndef PHLEX_MODEL_PRODUCTS_HPP
#define PHLEX_MODEL_PRODUCTS_HPP

#include "phlex/model/qualified_name.hpp"

#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <variant>

namespace phlex::experimental {

  struct product_base {
    virtual ~product_base() = default;
    virtual void const* address() const = 0;
    virtual std::type_index type() const = 0;
  };

  template <typename T>
  struct product : product_base {
    explicit product(T const& prod) : obj{prod} {}

    // The following constructor does NOT use a forwarding/universal reference!
    // It is not a template itself, but it uses the template parameter T from the
    // class template.
    explicit product(T&& prod) : obj{std::move(prod)} {}

    void const* address() const final { return &obj; }
    virtual std::type_index type() const { return std::type_index{typeid(T)}; }
    std::remove_cvref_t<T> obj;
  };

  class products {
    using collection_t = std::unordered_map<std::string, std::unique_ptr<product_base>>;

  public:
    using const_iterator = collection_t::const_iterator;

    template <typename T>
    void add(std::string const& product_name, T&& t)
    {
      add(product_name, std::make_unique<product<std::remove_cvref_t<T>>>(std::forward<T>(t)));
    }

    template <typename T>
    void add(std::string const& product_name, std::unique_ptr<product<T>>&& t)
    {
      products_.emplace(product_name, std::move(t));
    }

    template <typename Ts>
    void add_all(qualified_names const& names, Ts&& ts)
    {
      assert(names.size() == 1ull);
      add(names[0].name(), std::forward<Ts>(ts));
    }

    template <typename... Ts>
    void add_all(qualified_names const& names, std::tuple<Ts...> ts)
    {
      assert(names.size() == sizeof...(Ts));
      [this, &names]<std::size_t... Is>(auto const& ts, std::index_sequence<Is...>) {
        (this->add(names[Is].name(), std::get<Is>(ts)), ...);
      }(ts, std::index_sequence_for<Ts...>{});
    }

    template <typename T>
    T const& get(std::string const& product_name) const
    {
      auto it = products_.find(product_name);
      if (it == cend(products_)) {
        throw std::runtime_error("No product exists with the name '" + product_name + "'.");
      }

      // Should be able to use dynamic_cast a la:
      //
      //   if (auto t = dynamic_cast<product<T> const*>(it->second.get())) {
      //     return t->obj;
      //   }
      //
      // Unfortunately, this doesn't work well whenever products are inserted across
      // modules and shared object libraries.

      auto available_product = it->second.get();
      if (std::strcmp(typeid(T).name(), available_product->type().name()) == 0) {
        return reinterpret_cast<product<T> const*>(available_product)->obj;
      }

      throw_mismatched_type(product_name, typeid(T).name(), available_product->type().name());
    }

    bool contains(std::string const& product_name) const;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;

  private:
    static void throw_mismatched_type [[noreturn]] (std::string const& product_name,
                                                    char const* requested_type,
                                                    char const* available_type);

    collection_t products_;
  };
}

#endif // PHLEX_MODEL_PRODUCTS_HPP
