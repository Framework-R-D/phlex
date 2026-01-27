#ifndef PHLEX_CORE_PRODUCT_QUERY_HPP
#define PHLEX_CORE_PRODUCT_QUERY_HPP

#include "phlex/model/product_specification.hpp"
#include "phlex/model/type_id.hpp"

#include <optional>
#include <string>
#include <vector>

// This allows optional<std::string>s to be initialized using ""s syntax
using std::string_literals::operator""s;
namespace phlex {
  namespace detail {
    template <typename T>
      requires std::is_same_v<std::string, T> // has to be a template for static_assert(false)
    class required_creator_name {
    public:
      consteval required_creator_name()
      {
        static_assert(false, "The creator name has not been set in this product_query.");
      }
      required_creator_name(T&& rhs) : content_(std::forward<T>(rhs)) {}

      required_creator_name(std::string_view rhs) : content_(rhs) {}

      T&& release() { return std::move(content_); }

    private:
      std::string content_;
    };

    template <typename T>
      requires std::is_same_v<std::string, T> // has to be a template for static_assert(false)
    class required_layer_name {
    public:
      consteval required_layer_name()
      {
        static_assert(false, "The layer name has not been set in this product_query.");
      }
      required_layer_name(T&& rhs) : content_(std::forward<T>(rhs)) {}

      required_layer_name(std::string_view rhs) : content_(rhs) {}

      T&& release() { return std::move(content_); }

    private:
      std::string content_;
    };
  }

  struct product_tag {
    detail::required_creator_name<std::string> creator;
    detail::required_layer_name<std::string> layer;
    std::optional<std::string> suffix;
    std::optional<std::string> stage;
  };

  class product_query {
  public:
    product_query() = default; // Required by boost JSON
    product_query(product_tag&& tag) :
      creator_(tag.creator.release()),
      layer_(tag.layer.release()),
      suffix_(std::move(tag.suffix)),
      stage_(std::move(tag.stage))
    {
      if (creator_.empty()) {
        throw std::runtime_error("Cannot specify product with empty creator name.");
      }
      if (layer_.empty()) {
        throw std::runtime_error("Cannot specify the empty string as a data layer.");
      }
      update_hashes();
    }
    void set_type(experimental::type_id&& type);

    std::string const& creator() const noexcept { return creator_; }
    std::string const& layer() const noexcept { return layer_; }
    std::optional<std::string> const& suffix() const noexcept { return suffix_; }
    std::optional<std::string> const& stage() const noexcept { return stage_; }
    experimental::type_id const& type() const noexcept { return type_id_; }

    std::uint64_t creator_hash() const { return creator_hash_; }
    std::uint64_t suffix_hash() const { return suffix_hash_; }
    std::uint64_t type_hash() const { return type_hash_; }

    // Check that all products selected by /other/ would satisfy this query
    bool match(product_query const& other) const;

    // Check if a product_specification satisfies this query
    bool match(experimental::product_specification const& spec) const;

    std::string to_string() const;

    // temporary additional members for transition
    experimental::product_specification spec() const;
    bool operator==(product_query const& rhs) const
    {
      return this->spec() == rhs.spec() && this->layer_ == rhs.layer_;
    }

  private:
    std::string creator_;
    std::string layer_;
    std::optional<std::string> suffix_;
    std::optional<std::string> stage_;
    experimental::type_id type_id_;

    std::uint64_t creator_hash_ = 0;
    std::uint64_t suffix_hash_ = 0;
    std::uint64_t type_hash_ = 0;

    void update_hashes();
  };

  using product_queries = std::vector<product_query>;
  namespace detail {
    // C is a container of product_queries
    template <typename C, typename T>
      requires std::is_same_v<typename std::remove_cvref_t<C>::value_type, product_query> &&
               experimental::is_tuple<T>::value
    class product_queries_type_setter {};
    template <typename C, typename... Ts>
    class product_queries_type_setter<C, std::tuple<Ts...>> {
    private:
      std::size_t index_ = 0;

      template <typename T>
      void set_type(C& container)
      {
        container.at(index_).set_type(experimental::make_type_id<T>());
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
    requires std::is_same_v<typename std::remove_cvref_t<C>::value_type, product_query> &&
             experimental::is_tuple<Tup>::value
  void populate_types(C& container)
  {
    detail::product_queries_type_setter<decltype(container), Tup> populate_types{};
    populate_types(container);
  }
}
#endif // PHLEX_CORE_PRODUCT_QUERY_HPP
