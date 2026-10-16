#include "phlex/configuration.hpp"
#include "phlex/core/consumer.hpp"
#include "phlex/core/glue.hpp"
#include "phlex/core/join_registration.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/core/products_consumer.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/bulleted_list.hpp"

#include <boost/json/object.hpp>
#include <catch2/catch_test_macros.hpp>
#include <gsl/pointers>
#include <oneapi/tbb/flow_graph.h>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
  class test_products_consumer : public phlex::detail::products_consumer {
  public:
    test_products_consumer(oneapi::tbb::flow::graph& graph, phlex::product_selectors inputs) :
      products_consumer{
        "test_consumer", {}, std::move(inputs), graph, phlex::detail::require_layers::never},
      indices_{graph},
      completions_{graph}
    {
      for (std::size_t i = 0; i != num_inputs(); ++i) {
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
      for (std::size_t i = 0; i != num_inputs(); ++i) {
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

  SECTION("Default constructor")
  {
    algorithm_name const an;
    CHECK(an.to_string().empty());
  }
  SECTION("Create from string with colon")
  {
    auto an = algorithm_name::create("plugin:algo");
    CHECK(an.to_string() == "plugin:algo");
  }
  SECTION("Create from string without colon")
  {
    auto an = algorithm_name::create("algo");
    // For 'either' cases, the word is stored as algorithm_
    CHECK(an.to_string() == "algo");
  }
  SECTION("Create from char pointer")
  {
    auto an = algorithm_name::create("ptr:algo");
    CHECK(an.to_string() == "ptr:algo");
  }
  SECTION("Create error - trailing colon")
  {
    CHECK_THROWS_AS(algorithm_name::create("plugin:"), std::runtime_error);
  }
  SECTION("Match both")
  {
    algorithm_name const an = algorithm_name::create("p:a");
    CHECK(an.match(algorithm_name::create("p:a")));
    CHECK_FALSE(an.match(algorithm_name::create("p:b")));
  }
  SECTION("Bulleted list of algorithm_name")
  {
    std::vector<algorithm_name> const ans = {algorithm_name::create("p:a1"),
                                             algorithm_name::create("p:a2")};
    CHECK(bulleted_list(ans) == "  - p:a1\n  - p:a2");
  }
  SECTION("Empty bulleted list")
  {
    CHECK(bulleted_list(std::vector<identifier>{}).empty());
    CHECK(bulleted_list(std::vector<algorithm_name>{}).empty());
  }
}

TEST_CASE("consumer tests", "[core]")
{
  using namespace phlex::experimental::literals;
  auto an = phlex::experimental::algorithm_name::create("p:a");
  phlex::detail::consumer const c(an, {"pred1"});

  CHECK(c.name().to_string() == "p:a");
  CHECK(c.plugin() == "p"_idq);
  CHECK(c.algorithm() == "a"_idq);
  CHECK(c.when().size() == 1);
}

TEST_CASE("products_consumer checks layers and accepted cell paths", "[core][products_consumer]")
{
  using namespace phlex;
  using namespace phlex::detail;
  using namespace phlex::experimental;

  oneapi::tbb::flow::graph graph;
  auto const creator = algorithm_name::create("input");
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
  test_products_consumer consumer{graph, {{.layer = "run"}}};

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

TEST_CASE("products_consumer shares acceptance across registrations and ports per input",
          "[core][products_consumer]")
{
  using namespace phlex;
  using namespace phlex::detail;
  using namespace phlex::experimental;

  oneapi::tbb::flow::graph graph;
  auto const creator = algorithm_name::create("input");
  identifier const stage{"test_stage"};
  auto const run = data_cell_index::job()->make_child("run", 1);
  auto const store =
    std::make_shared<product_store>(run, gsl::not_null{&creator}, gsl::not_null{&stage});
  test_products_consumer consumer{
    graph, {{.layer = "run", .suffix = "left"}, {.layer = "run", .suffix = "right"}}};
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

TEST_CASE("verify_name tests", "[core]")
{
  using namespace phlex::detail::internal;

  SECTION("non-empty name does nothing") { CHECK_NOTHROW(verify_name("valid_name", nullptr)); }

  SECTION("empty name throws") { CHECK_THROWS_AS(verify_name("", nullptr), std::runtime_error); }

  SECTION("empty name with config includes module label")
  {
    boost::json::object obj;
    obj["module_label"] = "my_module";
    phlex::configuration const config{obj};

    try {
      verify_name("", &config);
      FAIL("Should have thrown");
    } catch (std::runtime_error const& e) {
      std::string const msg = e.what();
      CHECK(msg.contains("my_module"));
    }
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
