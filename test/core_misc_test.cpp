#include "phlex/configuration.hpp"
#include "phlex/core/consumer.hpp"
#include "phlex/core/declared_output.hpp"
#include "phlex/core/detail/make_algorithm_name.hpp"
#include "phlex/core/glue.hpp"
#include "phlex/core/join_registration.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/bulleted_list.hpp"

#include <boost/json/object.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <gsl/pointers>
#include <oneapi/tbb/flow_graph.h>

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
  class test_consumer : public phlex::detail::consumer {
  public:
    test_consumer(oneapi::tbb::flow::graph& graph, phlex::product_selectors inputs) :
      consumer{phlex::experimental::algorithm_name{"test_consumer"},
               {},
               std::move(inputs),
               graph,
               phlex::detail::require_layers::always},
      indices_{graph},
      completions_{graph}
    {
      for (std::size_t i = 0; i != input().size(); ++i) {
        products_.push_back(
          std::make_unique<oneapi::tbb::flow::queue_node<phlex::detail::message>>(graph));
      }
    }

    bool try_get(std::size_t input_ordinal, phlex::detail::message& msg)
    {
      return products_[input_ordinal]->try_get(msg);
    }

  private:
    oneapi::tbb::flow::receiver<phlex::detail::message>& port_for(
      phlex::product_selector const& input_product) override
    {
      return *products_[phlex::detail::port_index_for(input(), input_product)];
    }

    phlex::detail::join_registration index_ports_for() override
    {
      phlex::detail::join_registration result{.input_layers = layers(), .participants = {}};
      for (std::size_t i = 0; i != input().size(); ++i) {
        result.participants.push_back({.layer = layers()[i],
                                       .index_port = &indices_,
                                       .completion_port = &completions_,
                                       .input_ordinal = i});
      }
      return result;
    }

    std::size_t num_calls() const override { return 0; }

    std::vector<std::unique_ptr<oneapi::tbb::flow::queue_node<phlex::detail::message>>> products_;
    oneapi::tbb::flow::queue_node<phlex::detail::index_message> indices_;
    oneapi::tbb::flow::queue_node<phlex::detail::indexed_end_token> completions_;
  };
}

TEST_CASE("algorithm_name tests", "[model]")
{
  using namespace phlex::detail;
  using namespace phlex::experimental;
  using namespace phlex::experimental::literals;

  SECTION("Create from string with colon")
  {
    auto an = algorithm_name{"plugin:algo"};
    CHECK(an.to_string() == "plugin:algo");
  }
  SECTION("Create from string without colon")
  {
    auto an = algorithm_name{"algo"};
    // For 'either' cases, the word is stored as algorithm_
    CHECK(an.to_string() == "algo");
  }
  SECTION("Create from char pointer")
  {
    auto an = algorithm_name{"ptr:algo"};
    CHECK(an.to_string() == "ptr:algo");
  }
  SECTION("Unqualified name has no plugin")
  {
    auto const an = algorithm_name{"algo"};
    CHECK_FALSE(an.plugin().has_value());
    CHECK(an.algorithm() == "algo"_id);
  }
  SECTION("Qualified name has plugin and algorithm")
  {
    auto const an = algorithm_name{"plugin:algo"};
    REQUIRE(an.plugin().has_value());
    CHECK(an.plugin().value() == "plugin"_id);
    CHECK(an.algorithm() == "algo"_id);
  }
  SECTION("Construct from typed components")
  {
    algorithm_name const unqualified{std::nullopt, "algo"_id};
    CHECK_FALSE(unqualified.plugin().has_value());
    CHECK(unqualified.to_string() == "algo");
    CHECK(unqualified == algorithm_name{"algo"});

    algorithm_name const qualified{"plugin"_id, "algo"_id};
    REQUIRE(qualified.plugin().has_value());
    CHECK(qualified.to_string() == "plugin:algo");
    CHECK(qualified == algorithm_name{"plugin:algo"});
    CHECK(qualified != unqualified);
  }
  SECTION("Create error - structural problems are reported as invalid_argument")
  {
    auto const* const forms = R"( Expected "algorithm" or "plugin_label:algorithm".)";
    auto const message = [forms](std::string_view text, std::string_view reason) {
      return std::string{"Invalid algorithm name \""} + std::string{text} +
             "\": " + std::string{reason} + forms;
    };

    CHECK_THROWS_AS(algorithm_name{""}, std::invalid_argument);
    CHECK_THROWS_WITH(algorithm_name{""}, message("", "it is empty."));
    CHECK_THROWS_WITH(algorithm_name{"plugin:"},
                      message("plugin:", "the algorithm after ':' is missing."));
    CHECK_THROWS_WITH(algorithm_name{":algo"},
                      message(":algo", "the plugin label before ':' is missing."));
    CHECK_THROWS_WITH(algorithm_name{":"}, message(":", "the plugin label before ':' is missing."));
    CHECK_THROWS_WITH(algorithm_name{"a:b:c"}, message("a:b:c", "it contains more than one ':'."));
  }
  SECTION("Create error - malformed components are reported as invalid_argument")
  {
    auto const rule = std::string{phlex::detail::identifier_rule};

    CHECK_THROWS_AS(algorithm_name{"_x"}, std::invalid_argument);
    CHECK_THROWS_WITH(algorithm_name{"_x"},
                      "Invalid algorithm name \"_x\": algorithm \"_x\" " + rule);
    CHECK_THROWS_WITH(algorithm_name{"a[0]"},
                      "Invalid algorithm name \"a[0]\": algorithm \"a[0]\" " + rule);
    CHECK_THROWS_WITH(algorithm_name{"a b"},
                      "Invalid algorithm name \"a b\": algorithm \"a b\" " + rule);
    CHECK_THROWS_WITH(algorithm_name{"1p:a"},
                      "Invalid algorithm name \"1p:a\": plugin label \"1p\" " + rule);
    CHECK_THROWS_WITH(algorithm_name{"pl ugin:algo"},
                      "Invalid algorithm name \"pl ugin:algo\": plugin label \"pl ugin\" " + rule);
    CHECK_THROWS_WITH(algorithm_name{"p:1a"},
                      "Invalid algorithm name \"p:1a\": algorithm \"1a\" " + rule);
    // Only the first problem is reported, and the plugin label is checked first.
    CHECK_THROWS_WITH(algorithm_name{"1p:1a"},
                      "Invalid algorithm name \"1p:1a\": plugin label \"1p\" " + rule);
  }
  SECTION("Create error - long text is shortened in the message")
  {
    std::string const text = std::string(100, 'a') + "-";
    std::string const shown = std::string(64, 'a') + "...";
    CHECK_THROWS_WITH(algorithm_name{text},
                      Catch::Matchers::ContainsSubstring("\"" + shown + "\"") &&
                        !Catch::Matchers::ContainsSubstring(text));
  }
  SECTION("Create from optional plugin and algorithm")
  {
    CHECK(algorithm_name::create(std::nullopt, "algo") == algorithm_name{"algo"});
    CHECK(algorithm_name::create("plugin", "algo") == algorithm_name{"plugin:algo"});
    // An empty plugin is not the same as an absent plugin.
    CHECK_THROWS_AS(algorithm_name::create("", "algo"), std::invalid_argument);
    CHECK_THROWS_AS(algorithm_name::create(std::nullopt, ""), std::invalid_argument);
    CHECK_THROWS_AS(algorithm_name::create("plugin", ""), std::invalid_argument);
    // The overload does not split on colons, so a colon in a component violates the character rule.
    CHECK_THROWS_AS(algorithm_name::create(std::nullopt, "a:b"), std::invalid_argument);
  }
  SECTION("Match both")
  {
    algorithm_name const an = algorithm_name{"p:a"};
    CHECK(an.match(algorithm_name{"p:a"}));
    CHECK_FALSE(an.match(algorithm_name{"p:b"}));
  }
  SECTION("Bulleted list of algorithm_name")
  {
    std::vector<algorithm_name> const ans = {algorithm_name{"p:a1"}, algorithm_name{"p:a2"}};
    CHECK(bulleted_list(ans) == "  - p:a1\n  - p:a2");
  }
  SECTION("Empty bulleted list")
  {
    CHECK(bulleted_list(std::vector<identifier>{}).empty());
    CHECK(bulleted_list(std::vector<algorithm_name>{}).empty());
  }
}

TEST_CASE("consumer checks layers and accepted cell paths", "[core][consumer]")
{
  using namespace phlex;
  using namespace phlex::detail;
  using namespace phlex::experimental;

  oneapi::tbb::flow::graph graph;
  auto const creator = algorithm_name{"input"};
  identifier const stage{"test_stage"};
  auto const job = data_cell_index::job();
  auto const run = job->make_child("run", 1);
  auto const spill = job->make_child("spill", 1);
  auto const foreign_run = spill->make_child("run", 1);
  auto const make_store = [&](data_cell_index_ptr index) {
    return std::make_shared<product_store>(
      std::move(index), gsl::not_null{&creator}, gsl::not_null{&stage});
  };
  auto const run_store = make_store(run);
  auto const spill_store = make_store(spill);
  auto const foreign_store = make_store(foreign_run);
  test_consumer consumer{graph, {{.layer = "run"}}};

  // The checker must not retain a reference to this temporary selector.
  auto& port = consumer.port(product_selector{.layer = "run"});
  bool accepts_run = true;
  bool accepts_foreign = true;
  SECTION("Unset acceptance allows every matching layer") {}
  SECTION("Empty acceptance rejects every cell")
  {
    auto registration = consumer.index_ports();
    REQUIRE(registration.participants[0].accept_cells);
    registration.participants[0].accept_cells({});
    accepts_run = false;
    accepts_foreign = false;
  }
  SECTION("Configured acceptance excludes same-named foreign paths")
  {
    auto registration = consumer.index_ports();
    REQUIRE(registration.participants[0].accept_cells);
    // Include the wrong-layer hash too: membership must not bypass the layer check.
    registration.participants[0].accept_cells({run->layer_hash(), spill->layer_hash()});
    accepts_foreign = false;
  }

  REQUIRE(port.try_put({.store = run_store, .id = 1}));
  REQUIRE(port.try_put({.store = spill_store, .id = 2}));
  REQUIRE(port.try_put({.store = foreign_store, .id = 3}));
  graph.wait_for_all();
  message output;
  bool saw_run = false;
  bool saw_foreign = false;
  while (consumer.try_get(0, output)) {
    CHECK(output.store != spill_store);
    if (output.store == run_store) {
      CHECK(output.id == 1);
      saw_run = true;
    }
    if (output.store == foreign_store) {
      CHECK(output.id == 3);
      saw_foreign = true;
    }
  }
  CHECK(saw_run == accepts_run);
  CHECK(saw_foreign == accepts_foreign);
}

TEST_CASE("consumer shares acceptance across registrations and ports per input", "[core][consumer]")
{
  using namespace phlex;
  using namespace phlex::detail;
  using namespace phlex::experimental;

  oneapi::tbb::flow::graph graph;
  auto const creator = algorithm_name{"input"};
  identifier const stage{"test_stage"};
  auto const run = data_cell_index::job()->make_child("run", 1);
  auto const store =
    std::make_shared<product_store>(run, gsl::not_null{&creator}, gsl::not_null{&stage});
  test_consumer consumer{graph,
                         {{.layer = "run", .suffix = "left"}, {.layer = "run", .suffix = "right"}}};
  auto& left = consumer.port(consumer.input()[0]);
  auto& left_again = consumer.port(consumer.input()[0]);
  auto& right = consumer.port(consumer.input()[1]);
  auto first = consumer.index_ports();
  first.participants[0].accept_cells({});
  first.participants[1].accept_cells({run->layer_hash()});
  auto second = consumer.index_ports();

  REQUIRE(left.try_put({.store = store, .id = 1}));
  REQUIRE(left_again.try_put({.store = store, .id = 2}));
  REQUIRE(right.try_put({.store = store, .id = 3}));
  graph.wait_for_all();
  message output;
  CHECK_FALSE(consumer.try_get(0, output));
  REQUIRE(consumer.try_get(1, output));
  CHECK(output.id == 3);
  CHECK_FALSE(consumer.try_get(1, output));

  // Both registrations update the same consumer-owned state, including existing checkers.
  second.participants[0].accept_cells({run->layer_hash()});
  first.participants[1].accept_cells({});
  REQUIRE(left.try_put({.store = store, .id = 4}));
  REQUIRE(left_again.try_put({.store = store, .id = 5}));
  REQUIRE(right.try_put({.store = store, .id = 6}));
  graph.wait_for_all();
  std::size_t count = 0;
  while (consumer.try_get(0, output)) {
    CHECK((output.id == 4 or output.id == 5));
    CHECK(output.store == store);
    ++count;
  }
  CHECK(count == 2);
  CHECK_FALSE(consumer.try_get(1, output));
}

TEST_CASE("make_algorithm_name tests", "[core]")
{
  using namespace phlex::experimental;
  using namespace phlex::experimental::literals;
  using internal::make_algorithm_name;

  SECTION("Without configuration the plugin is absent")
  {
    auto const name = make_algorithm_name(nullptr, "my_algorithm");
    CHECK_FALSE(name.plugin().has_value());
    CHECK(name.algorithm() == "my_algorithm"_id);
  }

  SECTION("With configuration the module label is the plugin")
  {
    boost::json::object obj;
    obj["module_label"] = "my_module";
    phlex::configuration const config{obj};

    auto const name = make_algorithm_name(&config, "my_algorithm");
    REQUIRE(name.plugin().has_value());
    CHECK(name.plugin().value() == "my_module"_id);
    CHECK(name.to_string() == "my_module:my_algorithm");
  }

  SECTION("An empty algorithm name is rejected with the same message as create")
  {
    CHECK_THROWS_AS(make_algorithm_name(nullptr, ""), std::invalid_argument);
    CHECK_THROWS_WITH(
      make_algorithm_name(nullptr, ""),
      Catch::Matchers::ContainsSubstring("Invalid algorithm name \"\": it is empty"));

    boost::json::object obj;
    obj["module_label"] = "my_module";
    phlex::configuration const config{obj};
    CHECK_THROWS_WITH(
      make_algorithm_name(&config, ""),
      Catch::Matchers::ContainsSubstring(
        "Invalid algorithm name \"my_module:\": the algorithm after ':' is missing"));
  }

  SECTION("A malformed algorithm name is reported with the composed name")
  {
    boost::json::object obj;
    obj["module_label"] = "my_module";
    phlex::configuration const config{obj};

    CHECK_THROWS_AS(make_algorithm_name(&config, "my-algorithm"), std::invalid_argument);
    CHECK_THROWS_WITH(
      make_algorithm_name(&config, "my-algorithm"),
      Catch::Matchers::ContainsSubstring("Invalid algorithm name "
                                         "\"my_module:my-algorithm\": algorithm "
                                         "\"my-algorithm\" " +
                                         std::string{phlex::detail::identifier_rule}));
    CHECK_THROWS_AS(make_algorithm_name(nullptr, "a[0]"), std::invalid_argument);
  }

  SECTION("A malformed module label is reported as a bad plugin label")
  {
    boost::json::object obj;
    obj["module_label"] = "1module";
    phlex::configuration const config{obj};

    CHECK_THROWS_WITH(make_algorithm_name(&config, "my_algorithm"),
                      Catch::Matchers::ContainsSubstring(
                        "plugin label \"1module\" " + std::string{phlex::detail::identifier_rule}));
  }
}

TEST_CASE("add_to_error_messages tests", "[core]")
{
  using namespace phlex::detail::internal;

  std::vector<std::string> errors;
  add_to_error_messages(errors, "Node", "duplicate_node");

  REQUIRE(errors.size() == 1);
  CHECK(errors[0].contains("duplicate_node"));
}

TEST_CASE("Moving a registrar transfers node creation", "[core]")
{
  using namespace phlex::detail;

  tbb::flow::graph graph;
  declared_outputs outputs;
  std::vector<std::string> errors;
  int creations = 0;
  auto creator = [&](auto const&, auto const&) -> declared_output_ptr {
    ++creations;
    return std::make_unique<declared_output>(phlex::experimental::algorithm_name{"output"},
                                             1,
                                             std::vector<std::string>{},
                                             graph,
                                             [](auto const&) {});
  };

  SECTION("Move construction")
  {
    {
      registrar<declared_output_ptr> original{outputs, errors};
      original.set_creator(creator);
      auto moved = std::move(original);
    }
    CHECK(creations == 1);
    CHECK(outputs.get("output") != nullptr);
  }

  SECTION("Move assignment")
  {
    {
      registrar<declared_output_ptr> original{outputs, errors};
      original.set_creator(creator);
      registrar<declared_output_ptr> moved{outputs, errors};
      moved = std::move(original);
    }
    CHECK(creations == 1);
    CHECK(outputs.get("output") != nullptr);
  }

  CHECK(errors.empty());
}
