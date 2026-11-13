#include "data_products/waveform.hpp"

#include "phlex/core/product_selector.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/module.hpp"
#include "phlex/source.hpp"

#include <chrono>
#include <random>
#include <iostream>

using namespace phlex;

PHLEX_REGISTER_PROVIDERS(graph, config)
{
  int const seed = config.get<int>("seed", static_cast<int>(std::chrono::system_clock::now().time_since_epoch().count()));
  int const n_time_ticks = config.get<int>("n_time_ticks", 1024);

  std::mt19937 gen(seed);
  std::uniform_int_distribution<uint16_t> dist(20, 50);

  graph.provide("test_waveforms", [dist, gen, n_time_ticks](data_cell_index const& idx) mutable
  {
    std::vector<uint16_t> samples(n_time_ticks);
    for(auto& sample: samples) sample = dist(gen);

    return form::test::waveform(samples, idx.number());
  }).output_product("test_waveforms", phlex::experimental::identifier("test_waveforms"), phlex::experimental::identifier("wire"));
}

PHLEX_REGISTER_ALGORITHMS(graph, config)
{
  graph.observe("print_waveforms",
                [](form::test::waveform const& wfm) {
                  std::cout << "waveform:\n  channel = " << wfm.get_channel() << "\n  samples = {";
                   for(auto const& sample: wfm.get_samples()) std::cout << sample << ", ";
                   std::cout << "}" << std::endl;
                }).input_family(product_selector{.creator = "test_waveforms",
                                                 .layer = "wire",
                                                 .suffix = "test_waveforms"});
}
