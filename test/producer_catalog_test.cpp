#include "phlex/core/message.hpp"
#include "phlex/core/producer.hpp"
#include "phlex/core/producer_catalog.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"
#include "phlex/model/type_id.hpp"

#include <catch2/catch_test_macros.hpp>
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
    algorithm_name name_{"test_producer"};
    product_specifications products_{{name_, identifier{"number"}, make_type_id<int>()},
                                     {name_, identifier{"label"}, make_type_id<std::string>()}};
    tbb::flow::queue_node<message> port_;
  };
}

TEST_CASE("producer_catalog registers all products from a producer", "[producer_catalog]")
{
  tbb::flow::graph graph;
  test_producer node{graph};
  std::vector<producer*> producers{&node};
  producer_catalog const catalog{producers};

  CHECK(std::ranges::distance(catalog.values()) == 2);
  for (auto const& spec : node.output()) {
    phlex::product_selector const query{
      .creator = node.name().algorithm(), .suffix = spec.suffix(), .type = spec.type()};
    auto const matches =
      catalog.find_producers(query, algorithm_name{"consumer"}, identifier{"test_stage"});
    REQUIRE(matches.size() == 1u);
    CHECK(matches[0]->node == node.name());
    CHECK(matches[0]->output_port == &node.output_port());
    CHECK(matches[0]->type == spec.type());
  }
}
