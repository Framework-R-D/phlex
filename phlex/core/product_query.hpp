#ifndef PHLEX_CORE_PRODUCT_QUERY_HPP
#define PHLEX_CORE_PRODUCT_QUERY_HPP

#include "phlex/model/product_specification.hpp"
#include "phlex_core_export.hpp"

// #include <algorithm>
#include <string>
#include <vector>

namespace phlex {
  class phlex_core_EXPORT product_query {
  public:
    product_query(experimental::product_specification spec, std::string layer);

    auto const& spec() const noexcept { return spec_; }
    auto const& layer() const noexcept { return layer_; }
    void set_type(experimental::type_id&& type) { spec_.set_type(std::move(type)); }

    std::string to_string() const;

    auto operator<=>(product_query const&) const = default;

  private:
    experimental::product_specification spec_;
    std::string layer_;
  };

  using product_queries = std::vector<product_query>;
}

namespace phlex::experimental {
  struct phlex_core_EXPORT product_tag {
    product_specification spec;
    product_query operator()(std::string layer) &&;
  };

  namespace detail {
    // C is a container of product_queries
    template <typename C, typename T>
      requires std::is_same_v<typename std::remove_cvref_t<C>::value_type, product_query> &&
               is_tuple<T>::value
    class product_queries_type_setter {};
    template <typename C, typename... Ts>
    class product_queries_type_setter<C, std::tuple<Ts...>> {
    private:
      std::size_t index_ = 0;

      template <typename T>
      void set_type(C& container)
      {
        container.at(index_).set_type(make_type_id<T>());
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
             is_tuple<Tup>::value
  void populate_types(C& container)
  {
    detail::product_queries_type_setter<decltype(container), Tup> populate_types{};
    populate_types(container);
  }
}

namespace phlex {
  phlex_core_EXPORT experimental::product_tag operator""_in(char const* str, std::size_t);
}

#endif // PHLEX_CORE_PRODUCT_QUERY_HPP
