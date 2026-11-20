#ifndef TEST_RESOURCES_FOR_TESTING_HPP
#define TEST_RESOURCES_FOR_TESTING_HPP

namespace phlex::test {
  struct unlimited_test_resource {};

  struct pointer_token_resource {
    using token_type = pointer_token_resource const*;
  };

  struct second_pointer_token_resource {
    using token_type = second_pointer_token_resource const*;
  };

  struct value_token_resource {
    using token_type = value_token_resource;

    explicit value_token_resource(int v) : value{v} {}

    int value;
  };

  struct custom_token_resource {
    using token_type = int;

    static int token() { return 42; }
  };

  struct integer_unlimited_resource {
    explicit integer_unlimited_resource(int v) : value{v} {}

    int value;
  };

  struct marker_value_token_resource {
    using token_type = marker_value_token_resource;
  };
}

#endif // TEST_RESOURCES_FOR_TESTING_HPP
