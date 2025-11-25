#include "phlex/model/product_store.hpp"
#include "phlex/model/handle.hpp"


#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <tuple>
#include <vector>

using namespace phlex::experimental;

TEST_CASE("Product store insertion", "[data model]")
{
  auto store = product_store::base();
  constexpr int number = 4;
  std::vector many_numbers{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  store->add_product("number", number);
  store->add_product("numbers", many_numbers);

  CHECK_THROWS_WITH(
    store->get_product<double>("number"),
    Catch::Matchers::ContainsSubstring(
      "Cannot get product 'number' with type 'double' -- must specify type 'int'."));

  auto const matcher =
    Catch::Matchers::ContainsSubstring("No product exists with the name 'wrong_key'.");
  CHECK_THROWS_WITH(store->get_handle<int>("wrong_key"), matcher);

  CHECK(store->get_product<int>("number") == number);

  auto h = store->get_handle<std::vector<int>>("numbers");
  REQUIRE(h);
  CHECK(*h == many_numbers);
  CHECK(store->get_product<std::vector<int>>("numbers") == many_numbers);
}

TEST_CASE("Product store derivation", "[data model]")
{
  SECTION("Only one store")
  {
    auto store = product_store::base();
    auto stores = std::make_tuple(store);
    CHECK(store == more_derived(store, store));
    CHECK(store == most_derived(stores));
  }

  auto root = product_store::base();
  auto trunk = root->make_child(1, "trunk");
  SECTION("Compare different generations")
  {
    CHECK(trunk == more_derived(root, trunk));
    CHECK(trunk == more_derived(trunk, root));
  }
  SECTION("Compare siblings (right is always favored)")
  {
    auto bole = root->make_child(2, "bole");
    CHECK(bole == more_derived(trunk, bole));
    CHECK(trunk == more_derived(bole, trunk));
  }

  auto limb = trunk->make_child(2, "limb");
  auto branch = limb->make_child(3, "branch");
  auto twig = branch->make_child(4, "twig");
  auto leaf = twig->make_child(5, "leaf");

  auto order_a = std::make_tuple(root, trunk, limb, branch, twig, leaf);
  auto order_b = std::make_tuple(leaf, twig, branch, limb, trunk, root);
  auto order_c = std::make_tuple(twig, leaf, limb, branch, root, trunk);
  CHECK(leaf == most_derived(order_a));
  CHECK(leaf == most_derived(order_b));
  CHECK(leaf == most_derived(order_c));
}
