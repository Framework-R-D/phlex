// =======================================================================================
// This is a simple test to ensure that data products are "written" or "output" to an
// output node.
//
// N.B. Output nodes will eventually be replaced with preserver nodes.
// =======================================================================================

#include "phlex/core/framework_graph.hpp"
#include "phlex/core/source.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/products.hpp"
#include "plugins/layer_generator.hpp"

#include "catch2/catch_test_macros.hpp"

#include <ranges>
#include <set>
#include <string>

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

  detail::product_ptr give_me_a_name(data_cell_index const&)
  {
    return detail::product_for(brahms());
  }

  class test_source : public detail::source {
    detail::provider_bundles create_providers(product_selector const& selector) override
    {
      using namespace experimental;
      using namespace phlex::detail;
      provider_bundles bundles;
      std::string const layer = "spill";
      std::string const stage = "previous_process";
      product_specification spec{"provide_name", "", make_type_id<std::string>()};

      if (selector.match(spec, identifier{layer}, identifier{stage})) {
        bundles.push_back(phlex::detail::provider_bundle{.provider_function = give_me_a_name,
                                                         .max_concurrency = concurrency::unlimited,
                                                         .spec = std::move(spec),
                                                         .layer = layer,
                                                         .stage = stage});
      }
      return bundles;
    }
    index_generator indices() override { co_return; }
  };
}

TEST_CASE("Output data products", "[graph]")
{
  auto gen = experimental::layer_generator::make();
  gen->add_layer("spill", {"job", 1u});

  auto g = phlex::detail::framework_graph::without_driver();
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
