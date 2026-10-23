#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/products.hpp"
#include "phlex/model/type_id.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <functional>
#include <optional>
#include <set>
#include <unordered_set>

TEST_CASE("Reject null product pointers", "[data model]")
{
  using namespace phlex::experimental;
  CHECK_THROWS_WITH(product_for(product_ptr{}), "Cannot store a null product pointer.");
  CHECK_THROWS_WITH(product_for(nullptr), "Cannot store a null product pointer.");
}

TEST_CASE("Creator-less product specifications are exact keys", "[data model]")
{
  using namespace phlex::experimental;
  using namespace phlex::experimental::literals;
  using Catch::Matchers::ContainsSubstring;

  auto const type = make_type_id<int>();
  product_specification const creatorless{std::nullopt, "hits"_id, type};
  product_specification const creator_present{
    algorithm_name::create("reco:find_hits"), "hits"_id, type};
  product_specification const other_suffix{std::nullopt, "tracks"_id, type};

  SECTION("Absence is distinct from every present creator")
  {
    CHECK_FALSE(creatorless.has_creator());
    CHECK_FALSE(creatorless.creator().has_value());
    CHECK(creatorless.algorithm() == nullptr);
    CHECK(creatorless.plugin().empty());
    CHECK(creator_present.has_creator());
    CHECK(creator_present.plugin().text() == "reco");
    REQUIRE(creator_present.algorithm() != nullptr);
    CHECK(*creator_present.algorithm() == "find_hits"_id);

    CHECK(creatorless == product_specification{std::nullopt, "hits"_id, type});
    CHECK(creatorless != creator_present);
    CHECK(creatorless != other_suffix);
    CHECK((creatorless <=> creator_present) == std::strong_ordering::less);
    CHECK(std::hash<product_specification>{}(creatorless) ==
          std::hash<product_specification>{}(product_specification{std::nullopt, "hits"_id, type}));
    CHECK(std::hash<product_specification>{}(creatorless) !=
          std::hash<product_specification>{}(creator_present));

    std::set const ordered{creatorless, creator_present, other_suffix};
    CHECK(ordered.size() == 3ull);
    std::unordered_set const hashed{creatorless, creator_present, other_suffix};
    CHECK(hashed.size() == 3ull);
  }

  SECTION("Creator-less text omits the creator and slash")
  {
    CHECK(creatorless.to_string() == "hits");
    CHECK(creator_present.to_string() == "reco:find_hits/hits");
  }

  SECTION("Lookup is exact and a missing creator is not a wildcard")
  {
    phlex::detail::products products;
    products.add(creatorless, 1);
    CHECK(products.get<int>(creatorless) == 1);
    CHECK_THROWS_WITH(products.get<int>(creator_present),
                      ContainsSubstring("No product exists with the specification "
                                        "'reco:find_hits/hits'."));

    phlex::detail::products with_creator;
    with_creator.add(creator_present, 2);
    CHECK(with_creator.get<int>(creator_present) == 2);
    CHECK_THROWS_WITH(with_creator.get<int>(creatorless),
                      ContainsSubstring("No product exists with the specification 'hits'."));
    CHECK_THROWS_WITH(with_creator.get<int>(other_suffix),
                      ContainsSubstring("No product exists with the specification 'tracks'."));
  }
}
