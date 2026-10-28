#ifndef TEST_NODE_BUILDER_HELPERS_HPP
#define TEST_NODE_BUILDER_HELPERS_HPP

#include <functional>

namespace phlex::test {
  // The production node bodies adapt product messages and output ports before invoking the
  // registered algorithm. These tests use an int input, so direct invocation isolates resource
  // argument assembly from node-family-specific behavior.
  inline constexpr auto invoke_operation =
    [](auto const& operation, auto const& value, auto const&... resources) {
      std::invoke(operation, value, resources...);
    };
}

#endif // TEST_NODE_BUILDER_HELPERS_HPP
