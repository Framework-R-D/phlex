#ifndef PHLEX_CORE_PRODUCT_SELECTOR_HPP
#define PHLEX_CORE_PRODUCT_SELECTOR_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/model/type_id.hpp"

#include <concepts>
#include <format>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

// Used for the _id and _idq literals
using namespace phlex::experimental::literals;

namespace phlex {
  namespace detail {
    class creator_name {
    public:
      creator_name() : content_{std::nullopt} {}
      creator_name(std::optional<experimental::identifier>&& content) : content_{std::move(content)}
      {
        if (content_ && content_.value().empty()) {
          throw std::runtime_error("Cannot specify product with empty creator name.");
        }
      }
      template <typename U>
        requires std::constructible_from<experimental::identifier, U>
      // NOLINTNEXTLINE(google-explicit-constructor) - Implicit conversion is intentional
      creator_name(U&& rhs) : content_(std::forward<U>(rhs))
      {
        if (content_.value().empty()) {
          throw std::runtime_error("Cannot specify product with empty creator name.");
        }
      }

      operator bool() const noexcept { return content_.has_value(); }
      experimental::identifier const& operator*() const noexcept { return content_.operator*(); }
      friend experimental::identifier format_as(creator_name const& me) noexcept
      {
        return me.content_.value_or("[ANY]");
      }
      auto operator<=>(creator_name const&) const noexcept = default;

    private:
      std::optional<experimental::identifier> content_;
    };

    class layer_name {
    public:
      layer_name() : content_(std::nullopt) {}
      layer_name(std::optional<experimental::identifier>&& content) : content_{std::move(content)}
      {
        if (content_ && content_.value().empty()) {
          throw std::runtime_error("Cannot specify the empty string as a data layer.");
        }
      }
      template <typename U>
        requires std::constructible_from<experimental::identifier, U>
      // NOLINTNEXTLINE(google-explicit-constructor) - Implicit conversion is intentional
      layer_name(U&& rhs) : content_(std::forward<U>(rhs))
      {
        if (content_.value().empty()) {
          throw std::runtime_error("Cannot specify the empty string as a data layer.");
        }
      }

      // NOLINTNEXTLINE(google-explicit-constructor) - Implicit conversion is intentional
      operator experimental::identifier const&() const
      {
        if (!content_.has_value()) {
          throw std::logic_error("Cannot retrieve layer from product_selector with no layer");
        }
        return *content_;
      }

      experimental::identifier const& operator*() const noexcept { return content_.operator*(); }
      explicit operator std::string_view() const noexcept
      {
        using namespace std::string_view_literals;
        return content_
          .transform([](experimental::identifier const& id) { return std::string_view(id); })
          .value_or("[ANY]"sv);
      }
      operator bool() const noexcept { return content_.has_value(); }
      auto operator<=>(layer_name const&) const noexcept = default;

    private:
      std::optional<experimental::identifier> content_;
    };
  }

  struct PHLEX_CORE_EXPORT product_selector {
    detail::creator_name creator;
    detail::layer_name layer;
    std::optional<experimental::identifier> suffix;
    std::optional<experimental::identifier> stage;
    detail::type_id type;

    // Check that all products selected by /other/ would satisfy this query
    bool match(product_selector const& other) const;

    // Check if a product_specification satisfies this query
    bool match(detail::product_specification const& spec) const;

    // Check if a product_specification, layer, and stage together satisfies this query
    bool match(detail::product_specification const& spec,
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
               phlex::detail::is_tuple<T>::value
    struct product_selectors_type_setter {};
    template <typename C, typename... Ts>
    class product_selectors_type_setter<C, std::tuple<Ts...>> {
    private:
      std::size_t index_ = 0;

      template <typename T>
      void set_type(C& container)
      {
        container.at(index_).type = detail::make_type_id<T>();
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
             phlex::detail::is_tuple<Tup>::value
  void populate_types(C& container)
  {
    detail::product_selectors_type_setter<decltype(container), Tup> populate_types{};
    populate_types(container);
  }

  // This lives here rather than as a member-function of product_store because product_store is in model
  // and product_selector in core, with core depending on model.
  PHLEX_CORE_EXPORT detail::product_specification const* resolve_in_store(
    product_selector const& query, experimental::product_store const& store);
}

#endif // PHLEX_CORE_PRODUCT_SELECTOR_HPP
