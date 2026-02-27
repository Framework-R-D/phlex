// =======================================================================================
// This is a simple test to ensure that data products are "written" or "output" to an
// output node.
//
// N.B. Output nodes will eventually be replaced with preserver nodes.
// =======================================================================================

#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
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
      for (auto const& product_name : store | std::views::keys) {
        products_->insert(product_name);
      }
    }

  private:
    std::set<std::string>* products_;
  };
}

TEST_CASE("Output data products", "[graph]")
{
  experimental::layer_generator gen;
  gen.add_layer("spill", {"job", 1u});

  experimental::framework_graph g{driver_for_test(gen)};

  g.provide("provide_number", [](data_cell_index const&) -> int { return 17; })
    .output_product(product_query{
      .creator = "input"_id, .layer = "spill"_id, .suffix = "number_from_provider"_id});

  g.transform(
     "square_number",
     [](int const number) -> int { return number * number; },
     concurrency::unlimited)
    .input_family(product_query{
      .creator = "input"_id, .layer = "spill"_id, .suffix = "number_from_provider"_id})
    .output_products("squared_number");

  std::set<std::string> products_from_nodes;
  g.make<product_recorder>(products_from_nodes)
    .output("record_numbers", &product_recorder::record, concurrency::serial);

  g.execute();

  CHECK(g.execution_count("provide_number") == 1u);
  CHECK(g.execution_count("square_number") == 1u);
  // The "record_numbers" output node should be executed twice: once to receive the data
  // store from the "provide_number" provider, and once to receive the data store from the
  // "square_number" transform.
  CHECK(g.execution_count("record_numbers") == 2u);
  CHECK(products_from_nodes == std::set<std::string>{"number_from_provider", "squared_number"});
}
