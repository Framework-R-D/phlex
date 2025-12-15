#include "phlex/source.hpp"
#include "plugins/layer_generator.hpp"

#include "phlex/core/framework_graph.hpp"

#include "fmt/ranges.h"
#include "spdlog/spdlog.h"

#include <string>

using namespace phlex::experimental;

namespace {
  class generate_layers {
  public:
    generate_layers(configuration const& config)
    {
      auto const layers = config.get<configuration>("layers", {});
      for (auto const& key : layers.keys()) {
        auto const layer_config = layers.get<configuration>(key);
        auto const parent = layer_config.get<std::string>("parent", "job");
        auto const total_number = layer_config.get<unsigned int>("total");
        auto const starting_number = layer_config.get<unsigned int>("starting_number", 0);
        gen_.add_layer(key, {parent, total_number, starting_number});
      }

      // FIXME: Print out statement?
    }

    void next(framework_driver& driver) { gen_(driver); }

  private:
    layer_generator gen_;
  };
}

PHLEX_EXPERIMENTAL_REGISTER_SOURCE(generate_layers)
