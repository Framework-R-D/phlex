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

#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

using namespace phlex::experimental;
using namespace phlex::experimental::literals;

TEST_CASE("Product specification requires a creator", "[data model]")
{
  STATIC_CHECK_FALSE(std::is_default_constructible_v<product_specification>);
  STATIC_CHECK_FALSE(
    std::is_constructible_v<product_specification, std::nullopt_t, suffix_t, type_id>);
  STATIC_CHECK(
    std::is_same_v<std::remove_cvref_t<decltype(std::declval<product_specification>().creator())>,
                   algorithm_name>);
}

TEST_CASE("Product specification text requires creator and slash", "[data model]")
{
  SECTION("Creator with a named suffix")
  {
    auto const spec = product_specification::create("creator/number");
    CHECK(spec.creator() == algorithm_name::create("creator"));
    CHECK_FALSE(spec.creator().plugin().has_value());
    CHECK(spec.suffix() == "number"_id);
    CHECK_FALSE(spec.type().valid());
    CHECK(spec.to_string() == "creator/number");
  }
  SECTION("Plugin-qualified creator")
  {
    auto const spec = product_specification::create("plugin:creator/number");
    CHECK(spec.creator() == algorithm_name::create("plugin:creator"));
    CHECK(spec.suffix() == "number"_id);
    CHECK(spec.to_string() == "plugin:creator/number");
  }
  SECTION("Trailing slash means an explicit empty suffix")
  {
    auto const spec = product_specification::create("creator/");
    CHECK(spec.creator() == algorithm_name::create("creator"));
    CHECK(spec.suffix() == suffix_t{});
    CHECK(spec.suffix().empty());
    CHECK(spec.to_string() == "creator/");
    CHECK(product_specification::create(spec.to_string()) == spec);
  }
  SECTION("Implicit conversion from text")
  {
    product_specification const from_literal{"plugin:creator/number"};
    CHECK(from_literal == product_specification::create("plugin:creator/number"));
    product_specification const from_string{std::string{"creator/"}};
    CHECK(from_string == product_specification::create("creator/"));
    product_specification const from_view{std::string_view{"creator/number"}};
    CHECK(from_view == product_specification::create("creator/number"));
  }
  SECTION("Invalid forms are rejected")
  {
    CHECK_THROWS(product_specification::create(""));
    CHECK_THROWS(product_specification::create("number"));
    CHECK_THROWS(product_specification::create("creator"));
    CHECK_THROWS(product_specification::create("/number"));
    CHECK_THROWS(product_specification::create("/"));
    CHECK_THROWS(product_specification::create("plugin:/number"));
    CHECK_THROWS(product_specification::create(":creator/number"));
    CHECK_THROWS(product_specification::create("creator/a/b"));
    CHECK_THROWS(product_specification{"number"});
    CHECK_THROWS(product_specification{std::string{"/number"}});
  }
}

TEST_CASE("Product specification type mutation", "[data model]")
{
  auto spec = product_specification::create("creator/number");
  REQUIRE_FALSE(spec.type().valid());

  spec.set_type(make_type_id<int>());
  CHECK(spec.type() == make_type_id<int>());

  spec.set_type(make_type_id<double>());
  CHECK(spec.type() == make_type_id<double>());

  spec.set_type(type_id{});
  CHECK_FALSE(spec.type().valid());
  CHECK(spec == product_specification::create("creator/number"));
  CHECK(spec.creator() == algorithm_name::create("creator"));
  CHECK(spec.suffix() == "number"_id);
}

TEST_CASE("Product specification hash and equality", "[data model]")
{
  auto const creator = algorithm_name::create("plugin:producer");
  auto const unqualified = algorithm_name::create("producer");
  product_specification const named{creator, "number"_id, make_type_id<int>()};
  product_specification const unqualified_spec{unqualified, "number"_id, make_type_id<int>()};
  product_specification const untyped{creator, "number"_id, type_id{}};
  product_specification const empty_suffix{creator, suffix_t{}, make_type_id<int>()};
  std::hash<product_specification> const hash;

  CHECK(hash(named) == hash(product_specification{creator, "number"_id, make_type_id<int>()}));
  CHECK(hash(untyped) == hash(product_specification{creator, "number"_id, type_id{}}));
  CHECK(hash(empty_suffix) ==
        hash(product_specification{creator, suffix_t{}, make_type_id<int>()}));

  // Every component participates in identity; an unspecified type is not a wildcard.
  CHECK(named != unqualified_spec);
  CHECK(named != untyped);
  CHECK(named != empty_suffix);
  CHECK(named != product_specification{creator, "other"_id, make_type_id<int>()});
  CHECK(named != product_specification{creator, "number"_id, make_type_id<double>()});

  std::unordered_set<product_specification> specs;
  CHECK(specs.insert(named).second);
  CHECK(specs.insert(unqualified_spec).second);
  CHECK(specs.insert(untyped).second);
  CHECK(specs.insert(empty_suffix).second);
  CHECK_FALSE(
    specs.insert(product_specification{creator, "number"_id, make_type_id<int>()}).second);
  CHECK_FALSE(specs.insert(product_specification{creator, "number"_id, type_id{}}).second);
  CHECK(specs.size() == 4u);
  CHECK(specs.contains(product_specification{creator, "number"_id, make_type_id<int>()}));
  CHECK(specs.contains(product_specification{unqualified, "number"_id, make_type_id<int>()}));
  CHECK_FALSE(specs.contains(product_specification{creator, "other"_id, make_type_id<int>()}));
  CHECK_FALSE(specs.contains(product_specification{creator, "number"_id, make_type_id<double>()}));
  CHECK_FALSE(specs.contains(product_specification{
    algorithm_name::create("plugin:other"), "number"_id, make_type_id<int>()}));
}

TEST_CASE("Product store default source", "[data model]")
{
  auto const source = product_store::default_source();
  CHECK(source.to_string() == "Source");
  CHECK(source.algorithm() == "Source"_id);
  CHECK_FALSE(source.plugin().has_value());
  CHECK(product_store::default_source() == source);
}

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

TEST_CASE("Product retrieval requires exactly one match", "[data model]")
{
  using Catch::Matchers::ContainsSubstring;

  auto const creator = algorithm_name::create("test_algorithm");
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
