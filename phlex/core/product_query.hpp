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
    product_query operator()(std::string layer) &&;
    std::string to_string() const;

    static product_query create(char const* c);
    static product_query create(std::string const& s);
    static product_query create(product_query l);
  };

  using product_queries = std::vector<product_query>;

  inline auto& to_name(product_query const& label) { return label.name.name(); }
  inline auto& to_layer(product_query& label) { return label.layer; }

  product_query operator""_in(char const* str, std::size_t);
  bool operator==(product_query const& a, product_query const& b);
  bool operator!=(product_query const& a, product_query const& b);
  bool operator<(product_query const& a, product_query const& b);
  std::ostream& operator<<(std::ostream& os, product_query const& label);

  template <typename T>
  concept label_compatible = requires(T t) {
    { product_query::create(t) };
  };

  template <label_compatible T, std::size_t N>
  auto to_labels(std::array<T, N> const& like_labels)
  {
    std::array<product_query, N> labels;
    std::ranges::transform(
      like_labels, labels.begin(), [](T const& t) { return product_query::create(t); });
    return labels;
  }

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
