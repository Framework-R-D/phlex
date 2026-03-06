#include "phlex/source.hpp"
#include "form/config.hpp"
#include "form/form.hpp"
#include "form/technology.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

using namespace phlex;
using namespace phlex::experimental;

PHLEX_REGISTER_PROVIDERS(s, config)
{
  // --- Extract configuration ---
  std::string const input_file  = config.get<std::string>("input_file");
  std::string const creator     = config.get<std::string>("creator");
  std::string const tech_string = config.get<std::string>("technology", "ROOT_TTREE");
  auto const products           = config.get<std::vector<std::string>>("products");

  // --- Resolve technology enum ---
  std::unordered_map<std::string_view, int> const tech_lookup = {
    {"ROOT_TTREE",   form::technology::ROOT_TTREE},
    {"ROOT_RNTUPLE", form::technology::ROOT_RNTUPLE},
    {"HDF5",         form::technology::HDF5}};

  auto const it = tech_lookup.find(tech_string);
  if (it == tech_lookup.end()) {
    throw std::runtime_error("Unknown technology: " + tech_string);
  }
  int const technology = it->second;

  // --- Build input config ---
  form::experimental::config::output_item_config input_cfg;
  form::experimental::config::tech_setting_config tech_cfg;
  for (auto const& name : products) {
    input_cfg.addItem(name, input_file, technology);
  }

  // --- Create shared form_interface ---
  auto reader = std::make_shared<form::experimental::form_interface>(input_cfg, tech_cfg);

  // --- Register providers dynamically from config ---
  // FIXME: Prototype 0.1 -- types hardcoded as int.
  for (auto const& name : products) {
    s.provide("provide_" + name,
              [reader, creator, name](data_cell_index const& id) -> int {
                form::experimental::product_with_name pb{name, nullptr, &typeid(int)};
                reader->read(creator, id.to_string(), pb);
                if (!pb.data) {
                  throw std::runtime_error("FORM read returned null data for product: " + name);
                }
                return *static_cast<int const*>(pb.data);
              })
      .output_product(product_query{.creator = identifier(creator),
                                    .layer   = identifier("event"),
                                    .suffix  = identifier(name)});
  }
}
