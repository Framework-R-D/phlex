#include "phlex/concurrency.hpp"
#include "phlex/core/framework_graph.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/provider_node.hpp"
#include "phlex/core/source.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/handle.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/products.hpp"
#include "phlex/model/type_id.hpp"
#include "plugins/layer_generator.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <fmt/format.h>

#include <atomic>
#include <string>
#include <tuple>
#include <utility>

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

  experimental::product_ptr provide_archived_count(data_cell_index const& dci)
  {
    return experimental::product_for(static_cast<int>(dci.number()));
  }

  class archived_count_source : public source {
  public:
    provider_bundles create_providers(product_selector const& selector) override
    {
      using namespace experimental::literals;
      phlex::experimental::product_specification spec{
        "archived_input", "archived_count", phlex::experimental::make_type_id<int>()};
      if (!selector.match(spec, "event"_id, "previous_process"_id)) {
        return {};
      }
      return {{.provider_function = provide_archived_count,
               .max_concurrency = concurrency::unlimited,
               .spec = std::move(spec),
               .layer = "event",
               .stage = "previous_process"}};
    }
  };

  class copy_temperature_once {
  public:
    explicit copy_temperature_once(double const temperature) : temperature_{temperature} {}
    static bool initial_value() { return true; }
    static bool predicate(bool const emit) { return emit; }
    auto unfold(bool const) const { return std::pair{false, temperature_}; }

  private:
    double temperature_;
  };
}

TEST_CASE("Querying products in different ways", "[graph]")
{
  constexpr int num_events = 25;
  auto gen = experimental::layer_generator::make();
  gen->add_layer("event", {.parent_layer = "job", .count = num_events});
  auto g = phlex::detail::framework_graph::without_driver("test");
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

  SECTION("All fields")
  {
    g.transform("all_fields", [](int const& i) { return i + 1; })
      .input_family(product_selector{
        .creator = "input", .layer = "event", .suffix = "evt_number", .stage = "CURRENT"_id})
      .output_product_suffixes("event_number");
    g.execute();
    CHECK(g.execution_count("all_fields") == num_events);
  }

  SECTION("Graph stage name selects current producers")
  {
    g.transform("named_stage", [](int const& i) { return i + 1; })
      .input_family(product_selector{
        .creator = "input", .layer = "event", .suffix = "evt_number", .stage = "test"_id})
      .output_product_suffixes("event_number");
    g.execute();
    CHECK(g.execution_count("named_stage") == num_events);
  }

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
    CHECK_THROWS_WITH(
      g.unfold<copy_temperature_once>("copy_temperature_once",
                                      &copy_temperature_once::predicate,
                                      &copy_temperature_once::unfold,
                                      concurrency::unlimited,
                                      "temperature_copy")
        .input_family(product_selector{.creator = "input", .suffix = "temperature"})
        .output_product_suffixes("temperature"),
      "Must specify layers in the product selectors for node copy_temperature_once:\n"
      "  (Only invalid selectors are listed)\n"
      "  - <suffix temperature (of type <double>) by creator input (of stage [ANY]) in layer "
      "[ANY]>");
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

  SECTION("Products from this job, using layer only")
  {
    g.fold(
       "duplicate_temperature",
       [](std::atomic<double>& summary, double temp) { summary += temp; },
       concurrency::unlimited,
       "job")
      .input_family(product_selector{.creator = "input", .layer = "event"})
      .output_product_suffixes("temperature");

    g.transform("layer_only", [](double const& d) { return d; })
      .input_family(product_selector{.layer = "job"})
      .output_product_suffixes("job_temp");
    g.execute();
    CHECK(g.execution_count("layer_only") == 1);
  }

  SECTION("Ambiguous product selector with different suffixes is rejected")
  {
    g.transform("both_temperatures", [](double const& t) { return std::tuple{t, t}; })
      .input_family(product_selector{.creator = "input", .suffix = "temperature"})
      .output_product_suffixes("temperature_c", "temperature_f");
    g.observe("ambiguous_suffix", [](double const&) {})
      .input_family(product_selector{.creator = "both_temperatures"});
    CHECK_THROWS_WITH(g.execute(), "Ambiguous product selector -- see errors");
  }

  SECTION("Ambiguous product selector with different creators is rejected")
  {
    g.transform("another_temperature", [](double const& t) { return t; })
      .input_family(product_selector{.creator = "input", .suffix = "temperature"})
      .output_product_suffixes("temperature");
    g.observe("ambiguous_creator", [](double const&) {})
      .input_family(product_selector{.suffix = "temperature"});
    CHECK_THROWS_WITH(g.execute(), "Ambiguous product selector -- see errors");
  }
}

TEST_CASE("Selecting empty, named, and omitted producer suffixes", "[graph]")
{
  constexpr int num_events = 3;
  auto gen = experimental::layer_generator::make();
  gen->add_layer("event", {.parent_layer = "job", .count = num_events});
  auto g = phlex::detail::framework_graph::without_driver("test");
  g.add_driver(gen);
  g.provide("provide_number", provide_idx, concurrency::unlimited)
    .output_product("input", "number", "event");

  auto const input = product_selector{.creator = "input", .suffix = "number"};
  g.transform("both_suffixes", [](int const n) { return std::tuple{n + 1, n + 2}; })
    .input_family(input)
    .output_product_suffixes("", "number");

  SECTION("Explicit empty suffix selects only the empty product")
  {
    g.observe("selected",
              [](handle<int> const number) {
                CHECK(number.creator().algorithm == "both_suffixes");
                CHECK(number.suffix().empty());
                CHECK(*number == static_cast<int>(number.data_cell_index().number()) + 1);
              })
      .input_family(
        product_selector{.creator = "both_suffixes", .suffix = experimental::empty_string{}});
    g.execute();
    CHECK(g.execution_count("selected") == num_events);
  }

  SECTION("Named suffix selects only the named product")
  {
    g.observe("selected",
              [](handle<int> const number) {
                CHECK(number.creator().algorithm == "both_suffixes");
                CHECK(number.suffix() == "number");
                CHECK(*number == static_cast<int>(number.data_cell_index().number()) + 2);
              })
      .input_family(product_selector{.creator = "both_suffixes", .suffix = "number"});
    g.execute();
    CHECK(g.execution_count("selected") == num_events);
  }

  SECTION("Omitted suffix matches both empty and named products and is ambiguous")
  {
    g.observe("ambiguous_suffix", [](int const) {})
      .input_family(product_selector{.creator = "both_suffixes"});
    CHECK_THROWS_WITH(g.execute(), "Ambiguous product selector -- see errors");
  }

  SECTION("Omitted suffix selects a single default empty output")
  {
    g.transform("single_suffix", [](int const n) { return n; }).input_family(input);
    g.observe("selected",
              [](handle<int> const number) {
                CHECK(number.creator().algorithm == "single_suffix");
                CHECK(number.suffix().empty());
                CHECK(*number == static_cast<int>(number.data_cell_index().number()));
              })
      .input_family(product_selector{.creator = "single_suffix"});
    g.execute();
    CHECK(g.execution_count("selected") == num_events);
  }

  SECTION("Omitted suffix selects a single named output")
  {
    g.transform("single_suffix", [](int const n) { return n; })
      .input_family(input)
      .output_product_suffixes("number");
    g.observe("selected",
              [](handle<int> const number) {
                CHECK(number.creator().algorithm == "single_suffix");
                CHECK(number.suffix() == "number");
                CHECK(*number == static_cast<int>(number.data_cell_index().number()));
              })
      .input_family(product_selector{.creator = "single_suffix"});
    g.execute();
    CHECK(g.execution_count("selected") == num_events);
  }

  SECTION("Explicit empty suffix remains ambiguous across creators")
  {
    g.transform("another_suffix", [](int const n) { return n; })
      .input_family(input)
      .output_product_suffixes("");
    g.observe("ambiguous_creator", [](int const) {}).input_family(product_selector{.suffix = ""});
    CHECK_THROWS_WITH(g.execute(), "Ambiguous product selector -- see errors");
  }

  SECTION("Named suffix remains ambiguous across creators")
  {
    g.transform("another_suffix", [](int const n) { return n; })
      .input_family(input)
      .output_product_suffixes("number");
    g.observe("ambiguous_creator", [](int const) {})
      .input_family(product_selector{.suffix = "number"});
    CHECK_THROWS_WITH(g.execute(), "Ambiguous product selector -- see errors");
  }
}
