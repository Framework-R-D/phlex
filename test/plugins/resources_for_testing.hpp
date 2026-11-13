#ifndef TEST_PLUGINS_RESOURCES_FOR_TESTING_HPP
#define TEST_PLUGINS_RESOURCES_FOR_TESTING_HPP

#include <array>

namespace test::plugins {
  struct configured_resource {
    explicit configured_resource(int value) : value{value} {}

    int value;
  };

  struct serialized_resource {
    using token_type = serialized_resource const*;
  };

  struct pooled_resource {
    static std::array<int, 2> tokens() { return {1, 2}; }
  };
}

#endif // TEST_PLUGINS_RESOURCES_FOR_TESTING_HPP
