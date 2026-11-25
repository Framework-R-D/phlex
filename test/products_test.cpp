#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/products.hpp"
#include "phlex/model/type_id.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <compare>
#include <functional>
#include <set>
#include <unordered_set>

TEST_CASE("Reject null product pointers", "[data model]")
{
  using namespace phlex::experimental;
  CHECK_THROWS_WITH(product_for(product_ptr{}), "Cannot store a null product pointer.");
  CHECK_THROWS_WITH(product_for(nullptr), "Cannot store a null product pointer.");
}

TEST_CASE("Product specifications are exact keys", "[data model]")
{
  using namespace phlex::experimental;
  using namespace phlex::experimental::literals;
  using Catch::Matchers::ContainsSubstring;

  auto const type = make_type_id<int>();
  product_specification const unqualified{algorithm_name{"find_hits"}, "hits"_id, type};
  product_specification const qualified{algorithm_name{"reco:find_hits"}, "hits"_id, type};
  product_specification const other_suffix{algorithm_name{"find_hits"}, "tracks"_id, type};

  SECTION("An unqualified creator is distinct from a qualified creator")
  {
    CHECK_FALSE(unqualified.creator().plugin().has_value());
    REQUIRE(qualified.creator().plugin().has_value());
    CHECK(*qualified.creator().plugin() == "reco"_id);
    CHECK(qualified.creator().algorithm() == "find_hits"_id);

    CHECK(unqualified == product_specification{algorithm_name{"find_hits"}, "hits"_id, type});
    CHECK(unqualified != qualified);
    CHECK(unqualified != other_suffix);
    CHECK((unqualified <=> qualified) == std::strong_ordering::less);
    CHECK(std::hash<product_specification>{}(unqualified) ==
          std::hash<product_specification>{}(
            product_specification{algorithm_name{"find_hits"}, "hits"_id, type}));
    CHECK(std::hash<product_specification>{}(unqualified) !=
          std::hash<product_specification>{}(qualified));

    std::set const ordered{unqualified, qualified, other_suffix};
    CHECK(ordered.size() == 3ull);
    std::unordered_set const hashed{unqualified, qualified, other_suffix};
    CHECK(hashed.size() == 3ull);
  }

  SECTION("Text names the creator and the suffix")
  {
    CHECK(unqualified.to_string() == "find_hits/hits");
    CHECK(qualified.to_string() == "reco:find_hits/hits");
  }

  SECTION("Lookup is exact and a different creator is not a match")
  {
    phlex::detail::products products;
    products.add(unqualified, 1);
    CHECK(products.get<int>(unqualified) == 1);
    CHECK_THROWS_WITH(products.get<int>(qualified),
                      ContainsSubstring("No product exists with the specification "
                                        "'reco:find_hits/hits'."));

    phlex::detail::products with_plugin;
    with_plugin.add(qualified, 2);
    CHECK(with_plugin.get<int>(qualified) == 2);
    CHECK_THROWS_WITH(with_plugin.get<int>(unqualified),
                      ContainsSubstring("No product exists with the specification "
                                        "'find_hits/hits'."));
    CHECK_THROWS_WITH(with_plugin.get<int>(other_suffix),
                      ContainsSubstring("No product exists with the specification "
                                        "'find_hits/tracks'."));
  }
}
