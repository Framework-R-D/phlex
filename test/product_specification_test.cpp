#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/type_id.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_set>
#include <utility>

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
    auto const spec = product_specification{"creator/number"};
    CHECK(spec.creator() == algorithm_name{"creator"});
    CHECK_FALSE(spec.creator().plugin().has_value());
    CHECK(spec.suffix() == "number"_id);
    CHECK_FALSE(spec.type().valid());
    CHECK(spec.to_string() == "creator/number");
  }
  SECTION("Plugin-qualified creator")
  {
    auto const spec = product_specification{"plugin:creator/number"};
    CHECK(spec.creator() == algorithm_name{"plugin:creator"});
    CHECK(spec.suffix() == "number"_id);
    CHECK(spec.to_string() == "plugin:creator/number");
  }
  SECTION("Trailing slash means an explicit empty suffix")
  {
    auto const spec = product_specification{"creator/"};
    CHECK(spec.creator() == algorithm_name{"creator"});
    CHECK(spec.suffix() == suffix_t{});
    CHECK(spec.suffix().empty());
    CHECK(spec.to_string() == "creator/");
    CHECK(product_specification{spec.to_string()} == spec);
  }
  SECTION("Implicit conversion from text")
  {
    product_specification const from_literal{"plugin:creator/number"};
    CHECK(from_literal ==
          product_specification{algorithm_name{"plugin:creator"}, "number"_id, type_id{}});
    product_specification const from_string{std::string{"creator/"}};
    CHECK(from_string == product_specification{algorithm_name{"creator"}, suffix_t{}, type_id{}});
    product_specification const from_view{std::string_view{"creator/number"}};
    CHECK(from_view == product_specification{algorithm_name{"creator"}, "number"_id, type_id{}});
  }
  SECTION("Invalid forms are rejected")
  {
    CHECK_THROWS(product_specification{""});
    CHECK_THROWS(product_specification{"number"});
    CHECK_THROWS(product_specification{"creator"});
    CHECK_THROWS(product_specification{"/number"});
    CHECK_THROWS(product_specification{"/"});
    CHECK_THROWS(product_specification{"plugin:/number"});
    CHECK_THROWS(product_specification{":creator/number"});
    CHECK_THROWS(product_specification{"creator/a/b"});
    // Malformed creator text is reported by algorithm_name, as invalid_argument.
    CHECK_THROWS_AS(product_specification{"_x/number"}, std::invalid_argument);
    CHECK_THROWS_WITH(
      product_specification{"1p:a/number"},
      Catch::Matchers::ContainsSubstring("Invalid algorithm name \"1p:a\": "
                                         "plugin label \"1p\" " +
                                         std::string{phlex::detail::identifier_rule}));
    CHECK_THROWS(product_specification{std::string{"/number"}});
  }
}

TEST_CASE("Product specification type mutation", "[data model]")
{
  auto spec = product_specification{"creator/number"};
  REQUIRE_FALSE(spec.type().valid());

  spec.set_type(make_type_id<int>());
  CHECK(spec.type() == make_type_id<int>());

  spec.set_type(make_type_id<double>());
  CHECK(spec.type() == make_type_id<double>());

  spec.set_type(type_id{});
  CHECK_FALSE(spec.type().valid());
  CHECK(spec == product_specification{"creator/number"});
  CHECK(spec.creator() == algorithm_name{"creator"});
  CHECK(spec.suffix() == "number"_id);
}

TEST_CASE("Product specification hash and equality", "[data model]")
{
  auto const creator = algorithm_name{"plugin:producer"};
  auto const unqualified = algorithm_name{"producer"};
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
  CHECK_FALSE(specs.contains(
    product_specification{algorithm_name{"plugin:other"}, "number"_id, make_type_id<int>()}));
}
