#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"

#include "catch2/catch_test_macros.hpp"
#include "plugins/layer_generator.hpp"

#include "fmt/format.h"

#include <array>
#include <string>
#include <tuple>

using namespace phlex;
using namespace std::string_literals;

namespace {
  // Provider functions
  int provide_idx(data_cell_index const& dci) { return int(dci.number()); }
  int provide_number(data_cell_index const&) { return 3; }
  double provide_temperature(data_cell_index const& dci) { return double(dci.number()) * 100.0; }
  std::string provide_name(data_cell_index const& dci)
  {
    return fmt::format("John the {}th", dci.number());
  }
}

TEST_CASE("Querying products in different ways", "[graph]")
{
  constexpr int num_events = 25;
  experimental::layer_generator gen;
  gen.add_layer("event", {.parent_layer_name = "job", .total_per_parent_data_cell = num_events});
  experimental::framework_graph g{driver_for_test(gen)};

  // Register providers
  g.provide("provide_number_in_job", provide_number, concurrency::unlimited)
    .output_product(product_query{.creator = "input", .layer = "job", .suffix = "number"});
  g.provide("provide_number_in_event", provide_idx, concurrency::unlimited)
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "evt_number"});
  g.provide("provide_temperature_in_event", provide_temperature, concurrency::unlimited)
    .output_product(product_query{.creator = "input", .layer = "event", .suffix = "temperature"});
  g.provide("provide_temperature_in_event_again", provide_temperature, concurrency::unlimited)
    .output_product(
      product_query{.creator = "thermometer", .layer = "event", .suffix = "temperature"});
  g.provide("provide_name_in_event", provide_name, concurrency::unlimited)
    .output_product(product_query{.creator = "give_name", .layer = "event", .suffix = "name"});

  // Duplicate with transform
  g.transform("duplicate_temperature", [](double const& t) { return t; })
    .input_family(product_query{.creator = "input", .layer = "event", .suffix = "temperature"})
    .output_products("temperature");

  SECTION("All fields")
  {
    g.transform("all_fields", [](int const& i) { return i + 1; })
      .input_family(product_query{.creator = "input", .layer = "job", .suffix = "number"})
      .output_products("job_number");
    g.execute();
    CHECK(g.execution_count("all_fields") == 1);
  }

  SECTION("Creator and Layer, using creator (and using type alone)")
  {
    g.transform("creator_and_layer_by_creator", [](std::string const& str) { return str; })
      .input_family(product_query{.creator = "give_name", .layer = "event"})
      .output_products("event_name");
    g.observe(
       "verify_name",
       [](std::string const& str, int const& n) { CHECK(str == fmt::format("John the {}th", n)); })
      .input_family(product_query{.creator = "give_name", .layer = "event"},
                    product_query{.creator = "input", .layer = "event"});
    g.execute();
    CHECK(g.execution_count("creator_and_layer_by_creator") == num_events);
  }

  SECTION("Creator and Layer, using layer")
  {
    g.transform("creator_and_layer_by_layer", [](double const& d) { return d; })
      .input_family(product_query{.creator = "input", .layer = "event"})
      .output_products("event_temp");
    g.execute();
    CHECK(g.execution_count("creator_and_layer_by_layer") == num_events);
  }

  SECTION("Creator and Layer only, after transform")
  {
    g.transform("creator_and_layer_after_transform", [](double const& d) { return d; })
      .input_family(product_query{.creator = "duplicate_temperature", .layer = "event"})
      .output_products("event_temp");
    g.execute();
    CHECK(g.execution_count("creator_and_layer_after_transform") == num_events);
  }
}
