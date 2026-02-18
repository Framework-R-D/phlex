#include "phlex/core/detail/filter_impl.hpp"
#include "phlex/model/product_store.hpp"

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
  // This test does not exercise erasure of cached filter decisions
  decision_map decisions{2};

  SECTION("Test short-circuiting if false predicate result")
  {
    decisions.update({1, false});
    {
      auto const value = decisions.value(1);
      CHECK(is_complete(value));
      CHECK(to_boolean(value) == false);
    }
  }

  SECTION("Verify once a complete decision is made")
  {
    decisions.update({3, true});
    {
      auto const value = decisions.value(3);
      CHECK(not is_complete(value));
    }
    decisions.update({3, true});
    {
      auto const value = decisions.value(3);
      CHECK(is_complete(value));
      CHECK(to_boolean(value) == true);
    }
  }
}

TEST_CASE("Filter data map", "[filtering]")
{
  using phlex::product_query;
  std::vector const data_products_to_cache{
    product_query{.creator = "input"_id, .layer = "spill"_id, .suffix = "a"_id},
    product_query{.creator = "input"_id, .layer = "spill"_id, .suffix = "b"_id}};
  data_map data{data_products_to_cache};

  // Stores with the data products "a" and "b"
  auto store_with_a = product_store::base("provide_a");
  store_with_a->add_product("a", 1);
  auto store_with_b = product_store::base("provide_b");
  store_with_b->add_product("b", 2);

  std::size_t const msg_id{1};
  CHECK(not data.is_complete(msg_id));

  data.update(msg_id, store_with_a);
  CHECK(not data.is_complete(msg_id));

  data.update(msg_id, store_with_b);
  CHECK(data.is_complete(msg_id));
}
