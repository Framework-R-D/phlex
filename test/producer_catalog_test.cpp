#include "phlex/core/message.hpp"
#include "phlex/core/producer.hpp"
#include "phlex/core/producer_catalog.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/type_id.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <oneapi/tbb/flow_graph.h>

#include <iterator>
#include <string>
#include <vector>

using namespace phlex::detail;
using namespace phlex::experimental;

namespace {
  class test_producer : public producer {
  public:
    explicit test_producer(tbb::flow::graph& graph) : port_{graph} {}

    algorithm_name const& name() const noexcept override { return name_; }
    tbb::flow::sender<message>& output_port() override { return port_; }
    product_specifications const& output() const override { return products_; }

  private:
    algorithm_name name_{"test_plugin:test_producer"};
    product_specifications products_{{name_, empty_string{}, make_type_id<int>()},
                                     {name_, suffix_t{"number"}, make_type_id<int>()},
                                     {name_, suffix_t{"label"}, make_type_id<std::string>()}};
    tbb::flow::queue_node<message> port_;
  };
}

TEST_CASE("producer_catalog registers all products from a producer", "[producer_catalog]")
{
  tbb::flow::graph graph;
  test_producer node{graph};
  std::vector<producer*> producers{&node};
  producer_catalog const catalog{producers};

  CHECK(std::ranges::distance(catalog.values()) == 3);
  CHECK(node.output()[0].suffix().empty());
  CHECK(node.output()[1].suffix().text() == "number");
  for (auto const& spec : node.output()) {
    phlex::product_selector const query{
      .creator = node.name().algorithm(), .suffix = spec.suffix(), .type = spec.type()};
    auto const matches =
      catalog.find_producers(query, algorithm_name{"consumer"}, identifier{"test_stage"});
    REQUIRE(matches.size() == 1u);
    CHECK(matches[0]->node == node.name());
    CHECK(matches[0]->node.plugin() == identifier_or_empty{"test_plugin"});
    CHECK(matches[0]->output_port == &node.output_port());
    CHECK(matches[0]->type == spec.type());

    auto plugin_query = query;
    plugin_query.creator = "test_plugin";
    CHECK(catalog.find_producers(
            plugin_query, algorithm_name{"consumer"}, identifier{"test_stage"}) == matches);
  }

  // The empty and named int products differ only in suffix, so omission is ambiguous even
  // when the consumer's type is known.
  phlex::product_selector const ambiguous_suffix_query{.creator = node.name().algorithm(),
                                                       .type = make_type_id<int>()};
  CHECK_THROWS_WITH(catalog.find_producers(
                      ambiguous_suffix_query, algorithm_name{"consumer"}, identifier{"test_stage"}),
                    "Ambiguous product selector -- see errors");

  phlex::product_selector const omitted_suffix_query{.creator = node.name().algorithm(),
                                                     .type = make_type_id<std::string>()};
  auto const omitted_matches = catalog.find_producers(
    omitted_suffix_query, algorithm_name{"consumer"}, identifier{"test_stage"});
  REQUIRE(omitted_matches.size() == 1u);
  CHECK(omitted_matches[0]->type == make_type_id<std::string>());

  phlex::product_selector const missing_suffix_query{.suffix = "missing"};
  CHECK(
    catalog
      .find_producers(missing_suffix_query, algorithm_name{"consumer"}, identifier{"test_stage"})
      .empty());

  // Algorithm registration via framework_graph::input_family always populates
  // product_selector::type from the consumer callback's parameter types, so an
  // untyped selector matching products of different types can only be tested by
  // calling find_producers directly.
  phlex::product_selector const ambiguous_query{.creator = node.name().algorithm()};
  CHECK_THROWS_WITH(
    catalog.find_producers(ambiguous_query, algorithm_name{"consumer"}, identifier{"test_stage"}),
    "Ambiguous product selector -- see errors");
}
