#include "phlex/source.hpp"

#include "form/config.hpp"
#include "form/form.hpp"
#include "form/technology.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using namespace phlex;
using namespace phlex::experimental::literals;

PHLEX_REGISTER_PROVIDERS(s, config)
{
  std::cout << "Registering FORM source providers...\n";

  // --- Extract configuration ---
  std::string const input_file  = config.get<std::string>("input_file");
  std::string const creator     = config.get<std::string>("creator");
  std::string const tech_string = config.get<std::string>("technology", "ROOT_TTREE");

  std::cout << "Configuration:\n";
  std::cout << "  input_file:  " << input_file  << "\n";
  std::cout << "  creator:     " << creator     << "\n";
  std::cout << "  technology:  " << tech_string << "\n";

  // --- Resolve technology enum ---
  std::unordered_map<std::string_view, int> const tech_lookup = {
    {"ROOT_TTREE",   form::technology::ROOT_TTREE},
    {"ROOT_RNTUPLE", form::technology::ROOT_RNTUPLE},
    {"HDF5",         form::technology::HDF5}
  };

  auto it = tech_lookup.find(tech_string);
  if (it == tech_lookup.end()) {
    throw std::runtime_error("Unknown technology: " + tech_string);
  }
  int const technology = it->second;

  // --- Build input config ---
  form::experimental::config::output_item_config input_cfg;
  form::experimental::config::tech_setting_config tech_cfg;
  input_cfg.addItem("i", input_file, technology);
  input_cfg.addItem("j", input_file, technology);

  // --- Create shared form_interface ---
  auto reader = std::make_shared<form::experimental::form_interface>(input_cfg, tech_cfg);

  // --- Register providers ---
  // FIXME: Prototype 0.1 -- product names and types hardcoded.
  // product_query fields must be compile-time _id literals, not runtime strings.
  // To add new products, add new s.provide() blocks below following the same pattern.

  s.provide("provide_i",
            [reader, creator](data_cell_index const& id) -> int {
              std::cout << "\n=== form_source: providing i ===\n";
              form::experimental::product_with_name pb{"i", nullptr, &typeid(int)};
              reader->read(creator, id.to_string(), pb);
              if (!pb.data) {
                throw std::runtime_error("FORM read returned null data for product: i");
              }
              return *static_cast<int const*>(pb.data);
            })
    .output_product(product_query{
      .creator = "input"_id,
      .layer   = "event"_id,
      .suffix  = "i"_id
    });

  s.provide("provide_j",
            [reader, creator](data_cell_index const& id) -> int {
              std::cout << "\n=== form_source: providing j ===\n";
              form::experimental::product_with_name pb{"j", nullptr, &typeid(int)};
              reader->read(creator, id.to_string(), pb);
              if (!pb.data) {
                throw std::runtime_error("FORM read returned null data for product: j");
              }
              return *static_cast<int const*>(pb.data);
            })
    .output_product(product_query{
      .creator = "input"_id,
      .layer   = "event"_id,
      .suffix  = "j"_id
    });

  std::cout << "FORM source providers registered successfully\n";
}
