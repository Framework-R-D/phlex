#include "phlex/core/detail/filter_impl.hpp"

#include "catch2/catch_test_macros.hpp"

using namespace phlex::experimental;

TEST_CASE("Filter values", "[filtering]")
{
  CHECK(is_complete(true_value));
  CHECK(is_complete(false_value));
  CHECK(not is_complete(0u));
}

TEST_CASE("Filter decision", "[filtering]")
{
  decision_map decisions{2};
  decisions.update({nullptr, 1, false});
  {
    auto const value = decisions.value(1);
    CHECK(is_complete(value));
    CHECK(to_boolean(value) == false);
    decisions.erase(1);
  }
  decisions.update({nullptr, 3, true});
  {
    auto const value = decisions.value(3);
    CHECK(not is_complete(value));
  }
  decisions.update({nullptr, 3, true});
  {
    auto const value = decisions.value(3);
    CHECK(is_complete(value));
    CHECK(to_boolean(value) == true);
    decisions.erase(3);
  }
}
