#include "phlex/core/cached_product_stores.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"

using namespace phlex::experimental;

TEST_CASE("Cached product stores", "[data model]")
{
  cached_product_stores stores{};
  SECTION("Root store")
  {
    auto store = stores.get_store();
    REQUIRE(store);
    CHECK(store->id() == data_cell_index::base_ptr());
  }
  SECTION("One layer down")
  {
    auto store = stores.get_store("1"_id);
    REQUIRE(store);
    CHECK(*store->id() == *"1"_id);
    REQUIRE(store->parent());
    CHECK(store->parent()->id() == data_cell_index::base_ptr());
  }
  SECTION("One layer down, multiple instances")
  {
    auto store1 = stores.get_store("1"_id);
    auto store2 = stores.get_store("2"_id);
    REQUIRE(store1);
    REQUIRE(store2);
    CHECK(*store1->id() == *"1"_id);
    CHECK(*store2->id() == *"2"_id);
    // Make sure both stores have the same parent
    CHECK(store1->parent() == store2->parent());
  }
  SECTION("Multiple layers")
  {
    auto store1 = stores.get_store("1"_id);
    auto store2345 = stores.get_store("2:3:4:5"_id);
    REQUIRE(store1);
    REQUIRE(store2345);

    CHECK(*store1->id() == *"1"_id);
    auto store234 = store2345->parent();
    CHECK(*store234->id() == *"2:3:4"_id);
    auto store23 = store234->parent();
    CHECK(*store23->id() == *"2:3"_id);
    auto store2 = store23->parent();
    CHECK(*store2->id() == *"2"_id);

    // Make sure both stores have the same parent
    CHECK(store2->parent() == store1->parent());
  }
}
