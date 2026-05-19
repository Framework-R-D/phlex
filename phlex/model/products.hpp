#ifndef PHLEX_MODEL_PRODUCTS_HPP
#define PHLEX_MODEL_PRODUCTS_HPP

#include "phlex/phlex_model_export.hpp"

#include "phlex/model/product_specification.hpp"

#include <cassert>
#include <concepts>
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

namespace phlex::experimental {

  struct PHLEX_MODEL_EXPORT product_base {
    virtual ~product_base() = default;
    virtual void const* address() const = 0;
    virtual std::type_info const& type() const = 0;
  };
  using product_ptr = std::unique_ptr<product_base>;

  template <typename T>
  struct product : product_base {
    explicit product(T const& prod) : obj{prod} {}

    // The following constructor does NOT use a forwarding/universal reference!
    // It is not a template itself, but it uses the template parameter T from the
    // class template.
    explicit product(T&& prod) : obj{std::move(prod)} {}

    // The following two final overrides of virtual member functions are not
    // instantiated unless they are needed in some compilers (e.g. GCC and Clang), so unless
    // tested explicitly, compilation errors may hide until exposed by user code.
    // NOLINTBEGIN(portability-template-virtual-member-function)
    void const* address() const final { return &obj; }
    std::type_info const& type() const final { return typeid(T); }
    // NOLINTEND(portability-template-virtual-member-function)

    std::remove_cvref_t<T> obj;
  };

  template <typename T>
  product_ptr product_for(T&& t)
  {
    if constexpr (std::convertible_to<T, product_ptr>) {
      return std::forward<T>(t);
    } else {
      return std::make_unique<product<std::remove_cvref_t<T>>>(std::forward<T>(t));
    }
  }

  class PHLEX_MODEL_EXPORT products {
    using collection_t = std::vector<std::pair<product_specification, product_ptr>>;

  public:
    using const_iterator = collection_t::const_iterator;
    using size_type = collection_t::size_type;

    products() = default;
    explicit products(std::size_t number_known_products);

    template <typename T>
    void add(product_specification const& spec, T t)
    {
      products_.emplace_back(spec, product_for(std::move(t)));
    }

    template <typename Ts>
    void add_all(product_specifications const& names, Ts ts)
    {
      assert(names.size() == 1ull);
      add(names[0], std::move(ts));
    }

    template <typename... Ts>
    void add_all(product_specifications const& names, std::tuple<Ts...> ts)
    {
      assert(names.size() == sizeof...(Ts));
      [this, &names]<std::size_t... Is>(auto tuple, std::index_sequence<Is...>) {
        (this->add(names[Is], std::move(std::get<Is>(tuple))), ...);
      }(std::move(ts), std::index_sequence_for<Ts...>{});
    }

    template <typename T>
    T const& get(product_specification const& spec) const
    {
      auto const* available_product = find_product(spec);

      if (auto const* desired_product = dynamic_cast<product<T> const*>(available_product)) {
        return desired_product->obj;
      }

      throw_mismatched_type(spec, typeid(T).name(), available_product->type().name());
    }

    bool contains(product_specification const& spec) const;
    const_iterator begin() const noexcept;
    const_iterator end() const noexcept;
    size_type size() const noexcept;
    bool empty() const noexcept;

  private:
    product_base const* find_product(product_specification const& spec) const;
    static void throw_mismatched_type [[noreturn]] (product_specification const& spec,
                                                    char const* requested_type,
                                                    char const* available_type);

    collection_t products_;
  };
}

#endif // PHLEX_MODEL_PRODUCTS_HPP
