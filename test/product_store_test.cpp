#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_store.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <gsl/pointers>

#include <memory>
#include <tuple>
#include <vector>

using namespace phlex::experimental;
using namespace phlex::experimental::literals;

TEST_CASE("Product store insertion", "[data model]")
{
  auto const creator = algorithm_name::create("test_algorithm");
  auto const stage = "test_stage"_id;
  auto creator_ptr = gsl::make_not_null(&creator);
  auto stage_ptr = gsl::make_not_null(&stage);

  auto store = product_store::base(creator_ptr, stage_ptr);
  CHECK(store->empty());
  CHECK(store->stage() == stage);

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

  auto const creator = algorithm_name::create("test_algorithm");
  auto const stage = "test_stage"_id;
  auto creator_ptr = gsl::make_not_null(&creator);
  auto stage_ptr = gsl::make_not_null(&stage);

  SECTION("Only one store")
  {
    auto store = product_store::base(creator_ptr, stage_ptr);
    CHECK(store == more_derived(store, store));
    CHECK(store == most_derived(store));
  }

  auto root = product_store::base(creator_ptr, stage_ptr);
  auto trunk =
    std::make_shared<product_store>(root->index()->make_child("trunk", 1), creator_ptr, stage_ptr);
  CHECK(trunk->layer_name() == "trunk"_id);

  SECTION("Compare different generations")
  {
    CHECK(trunk == more_derived(root, trunk));
    CHECK(trunk == more_derived(trunk, root));
  }
  SECTION("Compare siblings (right is always favored)")
  {
    auto bole =
      std::make_shared<product_store>(root->index()->make_child("bole", 2), creator_ptr, stage_ptr);
    CHECK(bole == more_derived(trunk, bole));
    CHECK(trunk == more_derived(bole, trunk));
  }

  auto limb =
    std::make_shared<product_store>(trunk->index()->make_child("limb", 2), creator_ptr, stage_ptr);
  auto branch =
    std::make_shared<product_store>(limb->index()->make_child("branch", 3), creator_ptr, stage_ptr);
  auto twig =
    std::make_shared<product_store>(branch->index()->make_child("twig", 4), creator_ptr, stage_ptr);
  auto leaf =
    std::make_shared<product_store>(twig->index()->make_child("leaf", 5), creator_ptr, stage_ptr);

  auto order_a = std::make_tuple(root, trunk, limb, branch, twig, leaf);
  auto order_b = std::make_tuple(leaf, twig, branch, limb, trunk, root);
  auto order_c = std::make_tuple(twig, leaf, limb, branch, root, trunk);
  CHECK(leaf == most_derived(order_a));
  CHECK(leaf == most_derived(order_b));
  CHECK(leaf == most_derived(order_c));
}
