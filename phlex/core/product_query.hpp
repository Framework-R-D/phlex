#ifndef PHLEX_CORE_SPECIFIED_LABEL_HPP
#define PHLEX_CORE_SPECIFIED_LABEL_HPP

#include "phlex/model/product_specification.hpp"

#include <algorithm>
#include <array>
#include <iosfwd>
#include <string>
#include <vector>

namespace phlex::experimental {
  struct product_query {
    product_specification name;
    std::string layer;
    std::string to_string() const;
  };

  struct product_tag {
    product_specification name;
    product_query operator()(std::string layer) &&;
  };

  using product_queries = std::vector<product_query>;

  inline auto& to_name(product_query const& label) { return label.name.name(); }
  inline auto& to_layer(product_query& label) { return label.layer; }

  product_tag operator""_in(char const* str, std::size_t);
  bool operator==(product_query const& a, product_query const& b);
  bool operator!=(product_query const& a, product_query const& b);
  bool operator<(product_query const& a, product_query const& b);
  std::ostream& operator<<(std::ostream& os, product_query const& label);

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
        container.at(index_).name.set_type(make_type_id<T>());
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

#endif // PHLEX_CORE_SPECIFIED_LABEL_HPP
