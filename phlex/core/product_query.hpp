#ifndef PHLEX_CORE_PRODUCT_QUERY_HPP
#define PHLEX_CORE_PRODUCT_QUERY_HPP

#include "phlex/model/product_specification.hpp"

#include <algorithm>
#include <array>
#include <iosfwd>
#include <string>
#include <vector>

namespace phlex::experimental {
  struct product_query {
    product_specification spec;
    std::string layer;
    std::string to_string() const;
  };

  struct product_tag {
    product_specification spec;
    product_query operator()(std::string layer) &&;
    product_query in(std::string layer) &&
    {
      // std::move(*this) required to forward to rvalue ref qualified operator()
      return std::move(*this).operator()(std::move(layer));
    }
  };

  using product_queries = std::vector<product_query>;

  inline auto& to_name(product_query const& query) { return query.spec.name(); }
  inline auto& to_layer(product_query& query) { return query.layer; }

  product_tag from(char const* creator);
  product_tag operator""_in(char const* str, std::size_t);
  bool operator==(product_query const& a, product_query const& b);
  bool operator!=(product_query const& a, product_query const& b);
  bool operator<(product_query const& a, product_query const& b);
  std::ostream& operator<<(std::ostream& os, product_query const& query);

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
        container.at(index_).spec.set_type(make_type_id<T>());
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

#endif // PHLEX_CORE_PRODUCT_QUERY_HPP
