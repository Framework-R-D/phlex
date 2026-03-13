// ==============================================================================================
// See notes in plugins/layer_generator.hpp.  To configure the generate_layers driver to
// produce the tree shown in plugins/layer_generator.hpp, the user would configure the driver
// like (e.g.):
//
//   driver: {
//     plugin: "generate_layers",
//     layers: {
//       spill: { parent: "job", total: 16 },
//       CRU: { parent: "spill", total: 256 },
//       run: { parent: "job", total: 16},
//       APA: { parent: "run", total: 150, starting_number: 1 }
//     }
//   }
//
// Note that 'total' refers to the total number of data cells *per* parent.
// ==============================================================================================

#include "phlex/driver.hpp"
#include "plugins/layer_generator.hpp"

#include "phlex/core/framework_graph.hpp"

#include "fmt/ranges.h"
#include "spdlog/spdlog.h"

#include <string>

using namespace phlex;

PHLEX_EXPERIMENTAL_REGISTER_DRIVER(d, config)
{
  auto gen = std::make_shared<experimental::layer_generator>();
  auto const layers = config.get<configuration>("layers", {});
  for (auto const& key : layers.keys()) {
    auto const layer_config = layers.get<configuration>(key);
    auto const parent = layer_config.get<std::string>("parent", "job");
    auto const total_number = layer_config.get<unsigned int>("total");
    auto const starting_number = layer_config.get<unsigned int>("starting_number", 0);
    gen->add_layer(key, {parent, total_number, starting_number});
  }

  d.driver(gen->hierarchy(), [gen](framework_driver& driver) { (*gen)(driver); });
}
