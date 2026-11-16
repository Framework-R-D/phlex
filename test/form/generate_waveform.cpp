#include "data_products/waveform.hpp"

#include "phlex/core/product_selector.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/module.hpp"
#include "phlex/source.hpp"
#include "form/form_source_type_registry.hpp"

#include <chrono>
#include <random>
#include <iostream>

using namespace phlex;

PHLEX_REGISTER_PROVIDERS(graph, config)
{
  int const seed = config.get<int>("seed", static_cast<int>(std::chrono::system_clock::now().time_since_epoch().count()));
  int const n_time_ticks = config.get<int>("n_time_ticks", 1024);
  int const n_channels = config.get<int>("n_channels", 16);

  std::mt19937 gen(seed);
  std::uniform_int_distribution<uint16_t> dist(20, 50);

  graph.provide("test_waveforms", [dist, gen, n_time_ticks, n_channels](data_cell_index const& idx) mutable
  {
    std::vector<form::test::waveform> waveforms;
    for(int which_channel = 0; which_channel < n_channels; ++which_channel)
    {
      std::vector<uint16_t> samples(n_time_ticks);
      for(auto& sample: samples) sample = dist(gen);
      waveforms.emplace_back(samples, which_channel);
    }

    return waveforms;
  }).output_product("test_waveforms", phlex::experimental::identifier("test_waveforms"), phlex::experimental::identifier("apa"));
}

PHLEX_REGISTER_ALGORITHMS(graph, config)
{
  graph.observe("print_waveforms",
                [](std::vector<form::test::waveform> const& waveforms) {
                  for(auto const& wfm: waveforms)
                  {
                    std::cout << "waveform:\n  channel = " << wfm.get_channel() << "\n  samples = {";
                    for(auto const& sample: wfm.get_samples()) std::cout << sample << ", ";
                    std::cout << "}" << std::endl;
                  }
                }).input_family(product_selector{.creator = "test_waveforms",
                                                 .layer = "apa",
                                                 .suffix = "test_waveforms"});
  form::experimental::register_form_vector_product_type<form::test::waveform>("form::test::waveform");
}
