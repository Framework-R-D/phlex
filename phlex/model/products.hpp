#ifndef phlex_model_products_hpp
#define phlex_model_products_hpp

#include "phlex/model/level_id.hpp"
#include "phlex/model/qualified_name.hpp"

#include "boost/core/demangle.hpp"
#include "fmt/std.h"
#include "spdlog/spdlog.h"

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
    void add_all(std::array<qualified_name, 1> names, Ts&& ts)
    {
      add(names[0].name(), std::forward<Ts>(ts));
    }

    template <typename... Ts>
    void add_all(std::array<qualified_name, sizeof...(Ts)> names, std::tuple<Ts...> ts)
    {
      [this, &names]<std::size_t... Is>(auto const& ts, std::index_sequence<Is...>) {
        (this->add(names[Is].name(), std::get<Is>(ts)), ...);
      }(ts, std::index_sequence_for<Ts...>{});
    }

    template <typename T>
    std::variant<T const*, std::string> get(std::string const& product_name) const
    {
      auto it = products_.find(product_name);
      if (it == cend(products_)) {
        return "No product exists with the name '" + product_name + "'.";
      }

      // Should be able to use dynamic_cast a la:
      //
      //   if (auto t = dynamic_cast<product<T> const*>(it->second.get())) {
      //     return &t->obj;
      //   }
      //
      // Unfortunately, this doesn't work well whenever products are inserted across
      // modules and shared object libraries.

      auto available_product = it->second.get();
      if (std::strcmp(typeid(T).name(), available_product->type().name()) == 0) {
        return &reinterpret_cast<product<T> const*>(available_product)->obj;
      }
      return "Cannot get product '" + product_name + "' with type '" +
             boost::core::demangle(typeid(T).name()) + "' -- must specify type '" +
             boost::core::demangle(available_product->type().name()) + "'.";
    }

    bool contains(std::string const& product_name) const;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;

  private:
    collection_t products_;
  };
}

#endif // phlex_model_products_hpp
