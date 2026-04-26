#include "phlex/source.hpp"

#include "form/config.hpp"
#include "form/form_reader.hpp"
#include "form/technology.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

  class FormInputSource {
  public:
    FormInputSource(form::experimental::config::ItemConfig const& input_cfg,
                    form::experimental::config::tech_setting_config const& tech_cfg) :
      reader_(std::make_shared<form::experimental::form_reader_interface>(input_cfg, tech_cfg))
    {
    }

    template <typename T>
    T const& read(std::string const& creator,
           std::string const& product_name,
           phlex::data_cell_index const& id)
    {
      std::string const index_str = id.to_string();
      form::experimental::product_with_name pb{product_name, nullptr, &typeid(T)};
      reader_->read(creator, index_str, pb);
      if (!pb.data) {
        throw std::runtime_error("FORM Error: Failed to retrieve product [" + product_name +
                                 "] for " + index_str);
      }
      return *static_cast<T const*>(pb.data);
    }

  private:
    std::shared_ptr<form::experimental::form_reader_interface> reader_;
  };

}

PHLEX_REGISTER_PROVIDERS(s, config)
{
  std::cout << "Registering FORM input source...\n";

  // Extract configuration from Phlex config
  std::string const input_file = config.get<std::string>("input_file");
  std::string const creator = config.get<std::string>("creator");
  std::string const tech_string = config.get<std::string>("technology", "ROOT_TTREE");
  auto const products = config.get<std::vector<std::string>>("products");

  std::cout << "Configuration:\n";
  std::cout << "  input_file: " << input_file << "\n";
  std::cout << "  creator: " << creator << "\n";
  std::cout << "  technology: " << tech_string << "\n";

  std::unordered_map<std::string_view, int> const tech_lookup = {
    {"ROOT_TTREE", form::technology::ROOT_TTREE},
    {"ROOT_RNTUPLE", form::technology::ROOT_RNTUPLE},
    {"HDF5", form::technology::HDF5}};

  auto it = tech_lookup.find(tech_string);

  if (it == tech_lookup.end()) {
    throw std::runtime_error("Unknown technology: " + tech_string);
  }

  int const technology = it->second;

  // Build input config
  form::experimental::config::ItemConfig input_cfg;
  form::experimental::config::tech_setting_config tech_cfg;
  for (auto const& name : products) {
    input_cfg.addItem(name, input_file, technology);
  }

  // Create the FormInputSource object
  auto form_input = std::make_shared<FormInputSource>(input_cfg, tech_cfg);

  // --- Register providers dynamically from config ---
  // FIXME: Prototype 0.1 -- types hardcoded as int.
  for (auto const& name : products) {
    s.provide("provide_" + name,
              [form_input, creator, name](phlex::data_cell_index const& id) -> int {
                return form_input->read<int>(creator, name, id);
              })
      .output_product(phlex::product_query{.creator = phlex::experimental::identifier(creator),
                                           .layer = phlex::experimental::identifier("event"),
                                           .suffix = phlex::experimental::identifier(name)});
  }

  std::cout << "FORM input source registered successfully\n";
}
