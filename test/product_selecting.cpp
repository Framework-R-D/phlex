#include "phlex/core/framework_graph.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/source.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"
#include "plugins/layer_generator.hpp"

#include "fmt/format.h"

#include <array>
#include <string>
#include <tuple>

using namespace phlex;
using namespace std::string_literals;

namespace {
  // Provider functions
  int provide_idx(data_cell_index const& dci) { return static_cast<int>(dci.number()); }
  int provide_number(data_cell_index const&) { return 3; }
  double provide_temperature(data_cell_index const& dci)
  {
    return static_cast<double>(dci.number()) * 100.0;
  }
  std::string provide_name(data_cell_index const& dci)
  {
    return fmt::format("John the {}th", dci.number());
  }

  detail::product_ptr provide_archived_count(data_cell_index const& dci)
  {
    return std::make_unique<detail::product<int>>(static_cast<int>(dci.number()));
  }

  class archived_count_source : public source {
  public:
    detail::provider_bundles create_providers(product_selector const& selector) override
    {
      using namespace experimental::literals;
      phlex::detail::product_specification spec{
        "archived_input", "archived_count", phlex::detail::make_type_id<int>()};
      if (!selector.match(spec, "event"_id, "previous_process"_id)) {
        return {};
      }
      return {{.provider_function = provide_archived_count,
               .max_concurrency = concurrency::unlimited,
               .spec = std::move(spec),
               .layer = "event",
               .stage = "previous_process"}};
    }

    index_generator indices() override { co_return; }
  };

  class copy_temperature_once {
  public:
    explicit copy_temperature_once(double const temperature) : temperature_{temperature} {}
    bool initial_value() const { return true; }
    bool predicate(bool const emit) const { return emit; }
    auto unfold(bool const) const { return std::pair{false, temperature_}; }

  private:
    double temperature_;
  };
}

TEST_CASE("Querying products in different ways", "[graph]")
{
  constexpr int num_events = 25;
  auto gen = experimental::layer_generator::make();
  gen->add_layer("event", {.parent_layer_name = "job", .total_per_parent_data_cell = num_events});
  auto g = phlex::detail::framework_graph::without_driver();
  g.add_driver(gen);
  g.add_source<archived_count_source>("archived_count_source");

  // Register providers
  g.provide("provide_number_in_job", provide_number, concurrency::unlimited)
    .output_product("input", "number", "job");
  g.provide("provide_number_in_event", provide_idx, concurrency::unlimited)
    .output_product("input", "evt_number", "event");
  g.provide("provide_temperature_in_event", provide_temperature, concurrency::unlimited)
    .output_product("input", "temperature", "event");
  g.provide("provide_temperature_in_event_again", provide_temperature, concurrency::unlimited)
    .output_product("thermometer", "temperature", "event");
  g.provide("provide_name_in_event", provide_name, concurrency::unlimited)
    .output_product("give_name", "name", "event");

  // Duplicate with transform
  g.transform("duplicate_temperature", [](double const& t) { return t; })
    .input_family(product_selector{.creator = "input", .suffix = "temperature"})
    .output_product_suffixes("temperature");

  SECTION("Creator and suffix without layer")
  {
    g.transform("creator_and_suffix_without_layer", [](int const& i) { return i + 1; })
      .input_family(product_selector{.creator = "input", .suffix = "number"})
      .output_product_suffixes("job_number");
    g.execute();
    CHECK(g.execution_count("creator_and_suffix_without_layer") == 1);
  }

  SECTION("Creator and layer, distinguished by type")
  {
    g.transform("name_by_creator_and_layer", [](std::string const& str) { return str; })
      .input_family(product_selector{.creator = "give_name", .layer = "event"})
      .output_product_suffixes("event_name");
    g.observe(
       "verify_name",
       [](std::string const& str, int const& n) { CHECK(str == fmt::format("John the {}th", n)); })
      .input_family(product_selector{.creator = "give_name", .layer = "event"},
                    product_selector{.creator = "input", .layer = "event"});
    g.execute();
    CHECK(g.execution_count("name_by_creator_and_layer") == num_events);
  }

  SECTION("Layer alone, distinguished by type")
  {
    g.transform("name_by_layer_and_type", [](std::string const& str) { return str; })
      .input_family(product_selector{.layer = "event"})
      .output_product_suffixes("new_name");
    g.execute();
    CHECK(g.execution_count("name_by_layer_and_type") == num_events);
  }

  SECTION("Creator only without layer")
  {
    g.transform("temperature_by_creator_without_layer", [](double const& d) { return d; })
      .input_family(product_selector{.creator = "input"})
      .output_product_suffixes("event_temp");
    g.execute();
    CHECK(g.execution_count("temperature_by_creator_without_layer") == num_events);
  }

  SECTION("Creator only without layer, after transform")
  {
    g.transform("temperature_from_transform_without_layer", [](double const& d) { return d; })
      .input_family(product_selector{.creator = "duplicate_temperature"})
      .output_product_suffixes("event_temp");
    g.execute();
    CHECK(g.execution_count("temperature_from_transform_without_layer") == num_events);
  }

  SECTION("Predicate and observer inputs without layer")
  {
    g.predicate(
       "even_event_temperature",
       [](double const temperature) { return static_cast<int>(temperature / 100.0) % 2 == 0; },
       concurrency::unlimited)
      .input_family(product_selector{.creator = "input", .suffix = "temperature"});
    g.observe(
       "observe_even_event_temperature", [](double const) {}, concurrency::unlimited)
      .input_family(product_selector{.creator = "input", .suffix = "temperature"})
      .experimental_when("even_event_temperature");
    g.execute();
    CHECK(g.execution_count("even_event_temperature") == num_events);
    CHECK(g.execution_count("observe_even_event_temperature") == 13);
  }

  SECTION("Unfold inputs without layer are rejected")
  {
    g.unfold<copy_temperature_once>("copy_temperature_once",
                                    &copy_temperature_once::predicate,
                                    &copy_temperature_once::unfold,
                                    concurrency::unlimited,
                                    "temperature_copy")
      .input_family(product_selector{.creator = "input", .suffix = "temperature"})
      .output_product_suffixes("temperature");
    CHECK_THROWS_WITH(
      g.execute(),
      "<suffix temperature (of type <double>) by creator input (of stage [ANY]) in layer "
      "[ANY]> used by unfold copy_temperature_once has no layer defined");
  }

  SECTION("Shared implicit provider inputs without layer")
  {
    auto const archived_count =
      product_selector{.creator = "archived_input", .suffix = "archived_count"};
    g.transform("copy_archived_count", [](int const count) { return count; })
      .input_family(archived_count)
      .output_product_suffixes("archived_count_copy");
    g.observe(
       "observe_archived_count",
       [](handle<int> const count) { CHECK(count.stage() == "previous_process"); },
       concurrency::unlimited)
      .input_family(archived_count);
    g.execute();
    CHECK(g.execution_count("archived_input") == num_events);
    CHECK(g.execution_count("copy_archived_count") == num_events);
    CHECK(g.execution_count("observe_archived_count") == num_events);
  }
}
