#ifndef TEST_NODE_TEST_PRODUCTS_HPP
#define TEST_NODE_TEST_PRODUCTS_HPP

#include <compare>

namespace phlex::test {
  struct input_type_1 {
    int value;
    auto operator<=>(input_type_1 const&) const = default;
  };

  struct input_type_2 {
    int value;
    auto operator<=>(input_type_2 const&) const = default;
  };

  struct output_type_1 {
    int value;
    auto operator<=>(output_type_1 const&) const = default;
  };
}

#endif // TEST_NODE_TEST_PRODUCTS_HPP
