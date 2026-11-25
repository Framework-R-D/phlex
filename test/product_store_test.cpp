#include "phlex/core/input_arguments.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/model/type_id.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <gsl/pointers>

#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>

using namespace phlex::experimental;
using namespace phlex::experimental::literals;

TEST_CASE("Product store insertion", "[data model]")
{
  auto const creator = algorithm_name{"test_algorithm"};
  auto const stage = "test_stage"_id;
  auto creator_ptr = gsl::make_not_null(&creator);
  auto stage_ptr = gsl::make_not_null(&stage);

  auto store = product_store::base(creator_ptr, stage_ptr);
  CHECK(store->empty());
  CHECK(store->stage() == stage);

  constexpr int number = 4;
  std::vector many_numbers{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  store->add_product("test_algorithm/number", number);
  store->add_product("test_algorithm/numbers", many_numbers);
  product_specification const number_spec{"test_algorithm/number"};
  product_specification const numbers_spec{"test_algorithm/numbers"};
  product_specification const wrong_spec{"test_algorithm/wrong_key"};

  // Check number of products
  CHECK(store->size() == 2ull);

  CHECK_THROWS_WITH(
    store->get_product<double>(gsl::make_not_null(&number_spec)),
    Catch::Matchers::ContainsSubstring(
      "Cannot get product 'test_algorithm/number' with type 'double' -- must specify type 'int'."));

  auto const matcher = Catch::Matchers::ContainsSubstring(
    "No product exists with the specification 'test_algorithm/wrong_key'.");
  CHECK_THROWS_WITH(store->get_handle<int>(gsl::make_not_null(&wrong_spec)), matcher);

  CHECK(store->get_product<int>(gsl::make_not_null(&number_spec)) == number);

  auto h = store->get_handle<std::vector<int>>(gsl::make_not_null(&numbers_spec));
  REQUIRE(h);
  CHECK(*h == many_numbers);
  CHECK(h.suffix() == "numbers");
  CHECK(store->get_product<std::vector<int>>(gsl::make_not_null(&numbers_spec)) == many_numbers);
}

TEST_CASE("Product store lookup of specifications is exact", "[data model]")
{
  auto const creator = algorithm_name{"test_algorithm"};
  auto const stage = "test_stage"_id;
  auto store = product_store::base(gsl::make_not_null(&creator), gsl::make_not_null(&stage));

  auto const type = make_type_id<int>();
  product_specification const unqualified{creator, "hits"_id, type};
  product_specification const other_creator{algorithm_name{"reco:test_algorithm"}, "hits"_id, type};
  product_specification parsed{"test_algorithm/hits"};
  parsed.set_type(make_type_id<int>());

  store->add_product(unqualified, 1);

  // A parsed specification finds the same product once its type is supplied.
  CHECK(parsed == unqualified);
  CHECK(store->get_product<int>(gsl::make_not_null(&unqualified)) == 1);
  CHECK(store->get_product<int>(gsl::make_not_null(&parsed)) == 1);

  // A missing type is not a wildcard for a typed storage key.
  product_specification const untyped{"test_algorithm/hits"};
  CHECK(untyped != unqualified);
  CHECK_THROWS_WITH(store->get_product<int>(gsl::make_not_null(&untyped)),
                    Catch::Matchers::ContainsSubstring(
                      "No product exists with the specification 'test_algorithm/hits'."));

  // A different creator, even with the same algorithm and suffix, does not match.
  CHECK_THROWS_WITH(store->get_handle<int>(gsl::make_not_null(&other_creator)),
                    Catch::Matchers::ContainsSubstring(
                      "No product exists with the specification 'reco:test_algorithm/hits'."));
  CHECK(store->get_handle<int>(gsl::make_not_null(&unqualified)).suffix() == "hits");
}

TEST_CASE("Product store with the default source creator", "[data model]")
{
  auto const source = product_store::default_source();
  CHECK(source.to_string() == "phlex_source");
  CHECK_FALSE(source.plugin().has_value());
  CHECK(source.algorithm() == "phlex_source"_id);
  CHECK(source == algorithm_name{"phlex_source"});
  CHECK(product_store::default_source() == source);

  auto const stage = "test_stage"_id;
  auto store = product_store::base(gsl::make_not_null(&source), gsl::make_not_null(&stage));
  CHECK(store->source() == source);

  // Insertion of a product created by the source, and exact lookup by its specification.
  auto const type = make_type_id<int>();
  product_specification const from_source{source, "hits"_id, type};
  product_specification const other_creator{algorithm_name{"test_algorithm"}, "hits"_id, type};
  store->add_product(from_source, 7);

  CHECK(store->size() == 1ull);
  CHECK(store->get_product<int>(gsl::make_not_null(&from_source)) == 7);
  CHECK(store->get_handle<int>(gsl::make_not_null(&from_source)).suffix() == "hits");
  CHECK_THROWS_WITH(store->get_handle<int>(gsl::make_not_null(&other_creator)),
                    Catch::Matchers::ContainsSubstring(
                      "No product exists with the specification 'test_algorithm/hits'."));
}

TEST_CASE("Product retrieval requires exactly one match", "[data model]")
{
  using Catch::Matchers::ContainsSubstring;

  auto const creator = algorithm_name{"test_algorithm"};
  auto const stage = "test_stage"_id;
  auto store = product_store::base(gsl::make_not_null(&creator), gsl::make_not_null(&stage));
  phlex::detail::message const msg{.store = store};
  phlex::detail::retriever<int> const input{.query = {.type = make_type_id<int>()}};

  SECTION("No match in an empty store")
  {
    CHECK_THROWS_WITH(input.retrieve(msg), ContainsSubstring("No products found"));
  }

  product_specification const first{creator, "first"_id, make_type_id<int>()};
  product_specification const second{creator, "second"_id, make_type_id<int>()};
  product_specification const unrelated{creator, "unrelated"_id, make_type_id<double>()};
  store->add_product(first, 17);
  store->add_product(unrelated, 2.5);

  SECTION("No match lists the available products and store identity")
  {
    phlex::detail::retriever<int> const missing{
      .query = {.suffix = "missing"_id, .type = make_type_id<int>()}};
    CHECK_THROWS_WITH(
      missing.retrieve(msg),
      ContainsSubstring("No products found matching the query " + missing.query.to_string()) &&
        ContainsSubstring(store->index()->to_string()) && ContainsSubstring(creator.to_string()) &&
        ContainsSubstring(first.to_string()) && ContainsSubstring(unrelated.to_string()));
  }

  SECTION("A unique match returns the matching product handle")
  {
    auto const retrieved = input.retrieve(msg);
    REQUIRE(retrieved);
    CHECK(*retrieved == 17);
    CHECK(retrieved.suffix() == "first");
  }

  SECTION("Ambiguous matches list only the matching products")
  {
    store->add_product(second, 25);
    CHECK_THROWS_AS(input.retrieve(msg), std::runtime_error);
    CHECK_THROWS_WITH(
      input.retrieve(msg),
      ContainsSubstring("Multiple products found matching the query " + input.query.to_string()) &&
        ContainsSubstring(first.to_string()) && ContainsSubstring(second.to_string()) &&
        !ContainsSubstring(unrelated.to_string()));
  }
}

TEST_CASE("Product store derivation", "[data model]")
{
  using namespace phlex::experimental::detail;

  auto const creator = algorithm_name{"test_algorithm"};
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
