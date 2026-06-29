#include "plugins/layer_generator.hpp"
#include "phlex/core/framework_graph.hpp"

#include "catch2/catch_test_macros.hpp"
#include "catch2/matchers/catch_matchers_string.hpp"

using namespace phlex::experimental;
using namespace Catch::Matchers;

TEST_CASE("Only job layer", "[layer-generation]")
{
  auto gen = layer_generator::make();

  auto g = framework_graph::without_driver();
  g.add_driver(gen);
  g.execute();

  CHECK(gen->emitted_cell_count("/job") == 1);
  CHECK(gen->emitted_cell_count() == 1);
}

TEST_CASE("One non-job layer", "[layer-generation]")
{
  auto gen = layer_generator::make();
  gen->add_layer("spill", {"job", 16});

  auto g = framework_graph::without_driver();
  g.add_driver(gen);
  g.execute();

  CHECK(gen->emitted_cell_count("/job") == 1);
  CHECK(gen->emitted_cell_count("/job/spill") == 16);
  CHECK(gen->emitted_cell_count() == 1 + 16);
}

TEST_CASE("Two non-job layers", "[layer-generation]")
{
  auto gen = layer_generator::make();
  gen->add_layer("spill", {"job", 16});
  gen->add_layer("APA", {"spill", 16});

  auto g = framework_graph::without_driver();
  g.add_driver(gen);
  g.execute();

  CHECK(gen->emitted_cell_count("/job") == 1);
  CHECK(gen->emitted_cell_count("/job/spill") == 16);
  CHECK(gen->emitted_cell_count("/job/spill/APA") == 256);
  CHECK(gen->emitted_cell_count() == 1 + 16 + 256);
}

TEST_CASE("Test rebasing layers", "[layer-generation]")
{
  auto gen = layer_generator::make();
  gen->add_layer("APA", {"spill", 16});
  gen->add_layer("spill", {"job", 16});

  auto g = framework_graph::without_driver();
  g.add_driver(gen);
  g.execute();

  CHECK(gen->emitted_cell_count("/job") == 1);
  CHECK(gen->emitted_cell_count("/job/spill") == 16);
  CHECK(gen->emitted_cell_count("/job/spill/APA") == 256);
  CHECK(gen->emitted_cell_count() == 1 + 16 + 256);
}

TEST_CASE("Ambiguous layers", "[layer-generation]")
{
  auto gen = layer_generator::make();
  gen->add_layer("run", {"job", 16});
  gen->add_layer("spill", {"run", 16});
  gen->add_layer("spill", {"job", 16});

  CHECK_THROWS_WITH(gen->add_layer("APA", {"spill", 16}),
                    ContainsSubstring("Ambiguous: two parent layers found for data layer 'APA'") &&
                      ContainsSubstring("/job/run/spill") && ContainsSubstring("/job/spill"));

  // See following test that avoids ambiguity
}

TEST_CASE("Avoid ambiguous layers", "[layer-generation]")
{
  auto gen = layer_generator::make();
  gen->add_layer("run", {"job", 16});
  gen->add_layer("spill", {"run", 16});
  gen->add_layer("spill", {"job", 16});
  gen->add_layer("APA", {"/run/spill", 16}); // More complete parent path used to disambiguate

  auto g = framework_graph::without_driver();
  g.add_driver(gen);
  g.execute();

  CHECK(gen->emitted_cell_count("/job") == 1);
  CHECK(gen->emitted_cell_count("/job/run") == 16);
  CHECK(gen->emitted_cell_count("/job/run/spill") == 256);
  CHECK(gen->emitted_cell_count("/job/run/spill/APA") == 4096);
  CHECK(gen->emitted_cell_count("/job/spill") == 16);
  CHECK(gen->emitted_cell_count() == 1 + 16 + 256 + 4096 + 16);

  CHECK_THROWS_WITH(
    gen->emitted_cell_count("/job/spill/APA"),
    ContainsSubstring("No emitted cells corresponding to layer path '/job/spill/APA'"));
}
