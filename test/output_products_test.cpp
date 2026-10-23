// =======================================================================================
// This is a simple test to ensure that data products are "written" or "output" to an
// output node.
//
// N.B. Output nodes will eventually be replaced with preserver nodes.
// =======================================================================================

#include "phlex/concurrency.hpp"
#include "phlex/core/framework_graph.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/provider_node.hpp"
#include "phlex/core/source.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/model/products.hpp"
#include "phlex/model/type_id.hpp"
#include "plugins/layer_generator.hpp"

#include <catch2/catch_test_macros.hpp>

#include <compare>
#include <functional>
#include <optional>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

using namespace phlex;

namespace {
  class product_recorder {
  public:
    explicit product_recorder(std::set<std::string>& products) : products_{&products} {}

    void record(experimental::product_store const& store)
    {
      for (auto const& spec : store | std::views::keys) {
        products_->insert(spec.to_string());
      }
    }

  private:
    std::set<std::string>* products_;
  };

  constexpr std::string brahms() { return "Brahms"; }

  experimental::product_ptr give_me_a_name(data_cell_index const&)
  {
    return experimental::product_for(brahms());
  }

  class test_source : public detail::source {
    provider_bundles create_providers(product_selector const& selector) override
    {
      using namespace experimental;
      provider_bundles bundles;
      std::string const layer = "spill";
      std::string const stage = "previous_process";
      // The source's product has an explicitly empty suffix.
      experimental::product_specification spec{
        "provide_name", empty_string{}, experimental::make_type_id<std::string>()};

      if (selector.match(spec, identifier{layer}, identifier{stage})) {
        bundles.push_back(provider_bundle{.provider_function = give_me_a_name,
                                          .max_concurrency = concurrency::unlimited,
                                          .spec = std::move(spec),
                                          .layer = layer,
                                          .stage = stage});
      }
      return bundles;
    }
  };
}

TEST_CASE("Parse product specification text", "[data model]")
{
  using experimental::product_specification;
  using namespace experimental::literals;

  SECTION("A named suffix follows the creator and slash")
  {
    auto const spec = product_specification{"find_hits/hits"};
    CHECK(spec.creator().to_string() == "find_hits");
    CHECK_FALSE(spec.creator().plugin().has_value());
    CHECK(spec.suffix() == "hits"_id);
    CHECK(spec.to_string() == "find_hits/hits");
  }

  SECTION("A trailing slash gives an explicitly empty suffix")
  {
    auto const spec = product_specification{"find_hits/"};
    CHECK(spec.suffix().empty());
    CHECK(spec.to_string() == "find_hits/");
  }

  SECTION("A qualified creator is parsed")
  {
    auto const spec = product_specification{"reco:find_hits/hits"};
    CHECK(spec.creator().to_string() == "reco:find_hits");
    REQUIRE(spec.creator().plugin().has_value());
    CHECK(*spec.creator().plugin() == "reco"_id);
    CHECK(spec.suffix() == "hits"_id);
    CHECK(spec != product_specification{"find_hits/hits"});
  }

  SECTION("Text without a creator and slash is rejected")
  {
    for (auto const* text : {"hits", "/hits", "", "/"}) {
      CHECK_THROWS_AS(product_specification{text}, std::runtime_error);
    }
  }

  SECTION("An invalid creator is rejected")
  {
    CHECK_THROWS_AS(product_specification{"reco:/hits"}, std::invalid_argument);
    CHECK_THROWS_AS(product_specification{":find_hits/hits"}, std::invalid_argument);
  }
}

TEST_CASE("Product specification suffixes are explicit values", "[data model]")
{
  using experimental::algorithm_name;
  using experimental::empty_string;
  using experimental::product_specification;
  using experimental::suffix_t;
  using namespace experimental::literals;

  STATIC_CHECK(
    std::is_same_v<decltype(std::declval<product_specification>().suffix()), suffix_t const&>);

  SECTION("Parsed empty and named suffixes keep their representation")
  {
    auto const empty = product_specification{"reco:find_hits/"};
    CHECK(empty.suffix().empty());
    CHECK(empty.suffix() == suffix_t{empty_string{}});
    CHECK(empty.to_string() == "reco:find_hits/");

    auto const named = product_specification{"reco:find_hits/hits"};
    CHECK_FALSE(named.suffix().empty());
    CHECK(named.suffix() == suffix_t{"hits"_id});
    CHECK(named.suffix().text() == "hits");
    CHECK(named.to_string() == "reco:find_hits/hits");

    CHECK(empty != named);
    CHECK((empty <=> named) == std::strong_ordering::less);
    CHECK(std::hash<product_specification>{}(empty) != std::hash<product_specification>{}(named));
    CHECK(std::hash<product_specification>{}(empty) ==
          std::hash<product_specification>{}(product_specification{"reco:find_hits/"}));
  }

  SECTION("Constructed specifications accept the empty and named alternatives")
  {
    auto const creator = algorithm_name{"reco:find_hits"};
    product_specification const empty{creator, empty_string{}, experimental::make_type_id<int>()};
    product_specification const named{creator, "hits"_id, experimental::make_type_id<int>()};
    CHECK(empty.suffix().empty());
    CHECK(named.suffix().text() == "hits");
    CHECK(empty != named);
  }
}

TEST_CASE("Fluent output suffixes become explicit suffix values", "[data model]")
{
  using experimental::algorithm_name;
  using experimental::empty_string;
  using experimental::suffix_t;
  using namespace experimental::literals;

  auto const creator = algorithm_name{"reco:find_hits"};
  auto const int_id = experimental::make_type_id<int>();
  auto const double_id = experimental::make_type_id<double>();

  SECTION("No registered suffixes default to explicitly empty suffixes")
  {
    auto const specs = phlex::detail::to_product_specifications(creator, {}, {int_id, double_id});
    REQUIRE(specs.size() == 2ull);
    for (auto const& spec : specs) {
      CHECK(spec.suffix() == suffix_t{empty_string{}});
      CHECK(spec.suffix().empty());
      CHECK(spec.creator() == creator);
    }
    CHECK(specs[0].type() == int_id);
    CHECK(specs[1].type() == double_id);
    CHECK(specs[0].to_string() == "reco:find_hits/");
  }

  SECTION("Empty and named registered suffixes are converted at specification creation")
  {
    auto const specs =
      phlex::detail::to_product_specifications(creator, {"", "hits"}, {int_id, double_id});
    REQUIRE(specs.size() == 2ull);
    CHECK(specs[0].suffix() == suffix_t{empty_string{}});
    CHECK(specs[1].suffix() == suffix_t{"hits"_id});
    CHECK(specs[0].to_string() == "reco:find_hits/");
    CHECK(specs[1].to_string() == "reco:find_hits/hits");
    CHECK(specs[0] != specs[1]);
  }
}

TEST_CASE("Output data products", "[graph]")
{
  auto gen = experimental::layer_generator::make();
  gen->add_layer("spill", {.parent_layer = "job", .count = 1u});

  auto g = phlex::detail::framework_graph::without_driver("test");
  g.add_driver(gen);
  g.add_source<test_source>("test_source");

  g.provide("provide_number", [](data_cell_index const&) -> int { return 17; })
    .output_product("input", "number_from_provider", "spill");

  g.transform(
     "square_number",
     [](int const number) -> int { return number * number; },
     concurrency::unlimited)
    .input_family(
      product_selector{.creator = "input", .layer = "spill", .suffix = "number_from_provider"})
    .output_product_suffixes("squared_number");

  // Create another node that requires a data product from an implicit provider.
  // Concurrency must be serial because the CHECK macro cannot be invoked in a parallel context.
  g.observe(
     "read_name", [](std::string const& name) { CHECK(name == brahms()); }, concurrency::serial)
    .input_family(product_selector{.creator = "provide_name", .layer = "spill"});

  std::set<std::string> products_from_nodes;
  g.make<product_recorder>(products_from_nodes)
    .output("record_numbers", &product_recorder::record, concurrency::serial);

  g.execute();

  CHECK(g.execution_count("provide_number") == 1u);
  CHECK(g.execution_count("square_number") == 1u);

  // The "record_numbers" output node should be executed three times:
  //   - once to receive the data store from the "provide_number" explicit provider,
  //   - once to receive the data store from the "provide_name" implicit provider,
  //   - once to receive the data store from the "square_number" transform.
  CHECK(g.execution_count("record_numbers") == 3u);
  CHECK(products_from_nodes == std::set<std::string>{"input/number_from_provider",
                                                     "provide_name/",
                                                     "square_number/squared_number"});
}
