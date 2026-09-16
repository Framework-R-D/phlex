#include "phlex/model/handle.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_all.hpp"

#include <tuple>
#include <vector>

using namespace phlex::experimental;
using namespace phlex::experimental::literals;

TEST_CASE("Product store insertion", "[data model]")
{
  auto store = product_store::base("test_algorithm", "test_stage"_id);
  CHECK(store->empty());

  constexpr int number = 4;
  std::vector many_numbers{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  store->add_product("number", number);
  store->add_product("numbers", many_numbers);

  // Check number of products
  CHECK(store->size() == 2ull);

  CHECK_THROWS_WITH(
    store->get_product<double>("number"),
    Catch::Matchers::ContainsSubstring(
      "Cannot get product 'number' with type 'double' -- must specify type 'int'."));

  auto const matcher =
    Catch::Matchers::ContainsSubstring("No product exists with the specification 'wrong_key'.");
  CHECK_THROWS_WITH(store->get_handle<int>("wrong_key"), matcher);

  CHECK(store->get_product<int>("number") == number);

  auto h = store->get_handle<std::vector<int>>("numbers");
  REQUIRE(h);
  CHECK(*h == many_numbers);
  CHECK(store->get_product<std::vector<int>>("numbers") == many_numbers);
}

TEST_CASE("Product store derivation", "[data model]")
{
  using namespace phlex::experimental::detail;

  auto const dummy_creator_name = algorithm_name::create("test_algorithm");
  auto const dummy_stage_name = "test_stage"_id;

  SECTION("Only one store")
  {
    auto store = product_store::base(dummy_creator_name, dummy_stage_name);
    CHECK(store == more_derived(store, store));
    CHECK(store == most_derived(store));
  }

  auto root = product_store::base(dummy_creator_name, dummy_stage_name);
  auto trunk = std::make_shared<product_store>(
    root->index()->make_child("trunk", 1), dummy_creator_name, dummy_stage_name);
  SECTION("Compare different generations")
  {
    CHECK(trunk == more_derived(root, trunk));
    CHECK(trunk == more_derived(trunk, root));
  }
  SECTION("Compare siblings (right is always favored)")
  {
    auto bole = std::make_shared<product_store>(
      root->index()->make_child("bole", 2), dummy_creator_name, dummy_stage_name);
    CHECK(bole == more_derived(trunk, bole));
    CHECK(trunk == more_derived(bole, trunk));
  }

  auto limb = std::make_shared<product_store>(
    trunk->index()->make_child("limb", 2), dummy_creator_name, dummy_stage_name);
  auto branch = std::make_shared<product_store>(
    limb->index()->make_child("branch", 3), dummy_creator_name, dummy_stage_name);
  auto twig = std::make_shared<product_store>(
    branch->index()->make_child("twig", 4), dummy_creator_name, dummy_stage_name);
  auto leaf = std::make_shared<product_store>(
    twig->index()->make_child("leaf", 5), dummy_creator_name, dummy_stage_name);

  auto order_a = std::make_tuple(root, trunk, limb, branch, twig, leaf);
  auto order_b = std::make_tuple(leaf, twig, branch, limb, trunk, root);
  auto order_c = std::make_tuple(twig, leaf, limb, branch, root, trunk);
  CHECK(leaf == most_derived(order_a));
  CHECK(leaf == most_derived(order_b));
  CHECK(leaf == most_derived(order_c));
}
