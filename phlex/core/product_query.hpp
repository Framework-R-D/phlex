#ifndef PHLEX_CORE_PRODUCT_QUERY_HPP
#define PHLEX_CORE_PRODUCT_QUERY_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/model/type_id.hpp"

#include <concepts>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// Used for the _id and _idq literals
using namespace phlex::experimental::literals;

namespace phlex {
  namespace detail {
    // The required_creator_name has to be a template for static_assert(false)
    template <std::same_as<experimental::identifier> T>
    class required_creator_name {
    public:
      required_creator_name()
      {
        static_assert(false, "The creator name has not been set in this product_selector.");
      }
      template <typename U>
        requires std::constructible_from<T, U>
      // NOLINTNEXTLINE(google-explicit-constructor) - Implicit conversion is intentional
      required_creator_name(U&& rhs) : // NOLINT(cppcoreguidelines-missing-std-forward)
        content_(std::forward_like<T>(rhs))
      {
        if (content_.empty()) {
          throw std::runtime_error("Cannot specify product with empty creator name.");
        }
      }

      // NOLINTNEXTLINE(google-explicit-constructor) - Implicit conversion is intentional
      operator T const&() const noexcept { return content_; }
      bool operator==(required_creator_name const&) const noexcept = default;

    private:
      experimental::identifier content_;
    };

    // The required_layer_name has to be a template for static_assert(false)
    template <std::same_as<experimental::identifier> T>
    class required_layer_name {
    public:
      required_layer_name()
      {
        static_assert(false, "The layer name has not been set in this product_selector.");
      }
      template <typename U>
        requires std::constructible_from<T, U>
      // NOLINTNEXTLINE(google-explicit-constructor) - Implicit conversion is intentional
      required_layer_name(U&& rhs) : // NOLINT(cppcoreguidelines-missing-std-forward)
        content_(std::forward_like<T>(rhs))
      {
        if (content_.empty()) {
          throw std::runtime_error("Cannot specify the empty string as a data layer.");
        }
      }

      // NOLINTNEXTLINE(google-explicit-constructor) - Implicit conversion is intentional
      operator T const&() const noexcept { return content_; }
      bool operator==(required_layer_name const&) const noexcept = default;

    private:
      experimental::identifier content_;
    };
  }

  struct PHLEX_CORE_EXPORT product_selector {
    detail::required_creator_name<experimental::identifier> creator;
    detail::required_layer_name<experimental::identifier> layer;
    std::optional<experimental::identifier> suffix;
    std::optional<experimental::identifier> stage;
    experimental::type_id type;

    // Check that all products selected by /other/ would satisfy this query
    bool match(product_selector const& other) const;

    // Check if a product_specification satisfies this query
    bool match(experimental::product_specification const& spec) const;

    // Check if a product_specification, layer, and stage together satisfies this query
    bool match(experimental::product_specification const& spec,
               experimental::identifier const& layer,
               experimental::identifier const& stage) const;

    // Check if an algorithm name matches this query's creator
    bool creator_match(experimental::algorithm_name const& alg) const;

    std::string to_string() const;

    bool operator==(product_selector const& rhs) const;
    std::strong_ordering operator<=>(product_selector const& rhs) const;
  };

  inline std::string format_as(product_selector const& q) { return q.to_string(); }
  using product_selectors = std::vector<product_selector>;
  namespace detail {
    // C is a container of product_selectors
    template <typename C, typename T>
      requires std::is_same_v<typename std::remove_cvref_t<C>::value_type, product_selector> &&
               experimental::is_tuple<T>::value
    class product_selectors_type_setter {};
    template <typename C, typename... Ts>
    class product_selectors_type_setter<C, std::tuple<Ts...>> {
    private:
      std::size_t index_ = 0;

      template <typename T>
      void set_type(C& container)
      {
        container.at(index_).type = experimental::make_type_id<T>();
        ++index_;
      }

    public:
      void operator()(C& container)
      {
        assert(container.size() == sizeof...(Ts));
        (set_type<Ts>(container), ...);
      }
    };
  }

  template <typename Tup, typename C>
    requires std::is_same_v<typename std::remove_cvref_t<C>::value_type, product_selector> &&
             experimental::is_tuple<Tup>::value
  void populate_types(C& container)
  {
    detail::product_selectors_type_setter<decltype(container), Tup> populate_types{};
    populate_types(container);
  }

  // This lives here rather than as a member-function of product_store because product_store is in model
  // and product_selector in core, with core depending on model.
  PHLEX_CORE_EXPORT experimental::product_specification const* resolve_in_store(
    product_selector const& query, experimental::product_store const& store);
}

#endif // PHLEX_CORE_PRODUCT_QUERY_HPP
