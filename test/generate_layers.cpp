#include "phlex/source.hpp"
#include "test/layer_generator.hpp"

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
      auto layers = config.get<configuration>("layers");
      for (auto const& key : layers.keys()) {
        spdlog::info("Key: {}", key);
      }
    }

    void next(framework_driver& driver) const { gen_(driver); }

  private:
    layer_generator gen_;
  };
}

PHLEX_EXPERIMENTAL_REGISTER_SOURCE(generate_layers)
