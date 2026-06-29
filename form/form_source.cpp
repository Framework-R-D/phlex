#include "phlex/source.hpp"

#include "form/config.hpp"
#include "form/form_reader.hpp"
#include "form/form_source_type_registry.hpp"
#include "form/technology.hpp"

#include "phlex/model/data_cell_index.hpp"

#include <cassert>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

  phlex::data_cell_index_ptr parse_index_string(std::string const& index_string)
  {
    if (index_string == "[]") {
      return phlex::data_cell_index::job();
    }

    if (index_string.size() < 2 || index_string.front() != '[' || index_string.back() != ']') {
      throw std::runtime_error("Unsupported FORM index format: " + index_string);
    }

    auto current = phlex::data_cell_index::job();
    std::string const body = index_string.substr(1, index_string.size() - 2);
    std::stringstream ss{body};
    std::string component;
    while (std::getline(ss, component, ',')) {
      auto const first = component.find_first_not_of(' ');
      if (first == std::string::npos) {
        continue;
      }
      auto const trimmed = component.substr(first, component.find_last_not_of(' ') - first + 1);
      auto const colon = trimmed.find(':');
      if (colon == std::string::npos || colon == 0 || colon == trimmed.size() - 1) {
        throw std::runtime_error("Unsupported FORM index component: " + trimmed);
      }
      std::string const layer = trimmed.substr(0, colon);
      std::string const number_str = trimmed.substr(colon + 1);
      std::size_t number = 0;
      try {
        std::size_t pos = 0;
        number = std::stoull(number_str, &pos);
        if (pos != number_str.size()) {
          throw std::runtime_error("Unsupported FORM index component: " + trimmed);
        }
      } catch (std::exception const&) {
        throw std::runtime_error("Unsupported FORM index component: " + trimmed);
      }
      current = current->make_child(layer, number);
    }
    return current;
  }

  class FormInputSource : public phlex::experimental::source {
  public:
    FormInputSource(form::experimental::config::ItemConfig const& input_cfg,
                    form::experimental::config::tech_setting_config const& tech_cfg,
                    std::string const& actual_creator,
                    std::string const& advertised_creator,
                    std::vector<std::string> const& products) :
      reader_(std::make_shared<form::experimental::form_reader_interface>(input_cfg, tech_cfg)),
      actual_creator_(actual_creator),
      advertised_creator_(advertised_creator),
      products_(products)
    {
      // Ensure all builtin types are registered for dynamic dispatch
      form::experimental::ensure_builtin_form_product_types_registered();
    }

    phlex::experimental::provider_bundles create_providers(
      phlex::product_selector const& selector) override
    {
      using namespace phlex::experimental;
      provider_bundles bundles;

      std::string const* product_type_name =
        form::experimental::find_form_product_type_name(selector.type);
      if (product_type_name == nullptr) {
        return bundles;
      }

      auto const* selected_entry = form::experimental::find_form_product_type(*product_type_name);
      if (selected_entry == nullptr || selected_entry->cpp_type == nullptr) {
        return bundles;
      }

      for (auto const& name : products_) {
        product_specification spec{
          algorithm_name::create(advertised_creator_), identifier{name}, selector.type};

        // Use selector's layer and stage; stage defaults to "event" if not specified
        identifier const selector_layer = selector.layer;
        identifier const selector_stage = selector.stage.value_or(identifier{"event"});

        if (!selector.match(spec, selector_layer, selector_stage)) {
          continue;
        }

        reader_->prime(actual_creator_, name, *selected_entry->cpp_type);

        auto provider_func =
          [this, name, product_type = *product_type_name](
            phlex::data_cell_index const& id) -> phlex::experimental::product_ptr {
          return this->read_product_from_form(actual_creator_, name, id.to_string(), product_type);
        };

        bundles.push_back(
          provider_bundle{.provider_function = std::function<provider_function_t>(provider_func),
                          .max_concurrency = phlex::concurrency::serial,
                          .spec = std::move(spec),
                          .layer = std::string(selector_layer.trans_get_string()),
                          .stage = std::string(selector_stage.trans_get_string())});
      }

      return bundles;
    }

    // TODO: replace with per-container index lookup driven by metadata payload.
    phlex::index_generator indices() override
    {
      if (products_.empty()) {
        co_return;
      }

      for (auto const& index_string : reader_->indices(actual_creator_, products_.front())) {
        co_yield parse_index_string(index_string);
      }
    }

    phlex::experimental::product_ptr read_product_from_form(std::string const& creator,
                                                            std::string const& product_name,
                                                            std::string const& index_str,
                                                            std::string const& product_type)
    {
      form::experimental::form_source_type_entry const* entry =
        form::experimental::find_form_product_type(product_type);
      if (entry && entry->cpp_type && entry->product_from_data_fn) {
        form::experimental::product_with_name pb{product_name, nullptr, entry->cpp_type};
        reader_->read(creator, index_str, pb);
        return entry->product_from_data_fn(pb.data, product_name, index_str);
      }
      throw std::runtime_error("Unsupported FORM product type: " + product_type);
    }

  private:
    std::shared_ptr<form::experimental::form_reader_interface> reader_;
    std::string actual_creator_;
    std::string advertised_creator_;
    std::vector<std::string> products_;
  };
}

PHLEX_REGISTER_SOURCE(s, config)
{
  std::cout << "Registering FORM input source...\n";

  std::string const input_file = config.get<std::string>("input_file");
  std::string const advertised_creator = config.get<std::string>("creator");
  std::string const tech_string = config.get<std::string>("technology", "ROOT_TTREE");
  std::string const module_label = config.get<std::string>("module_label", "form_source");
  auto const products = config.get<std::vector<std::string>>("products");

  std::string actual_creator = advertised_creator;
  auto const algorithm = config.get_if_present<std::string>("algorithm");
  auto const plugin = config.get_if_present<std::string>("plugin");
  if (algorithm && plugin && !algorithm->empty() && !plugin->empty()) {
    actual_creator = *plugin + ":" + *algorithm;
  }

  std::unordered_map<std::string_view, int> const tech_lookup = {
    {"ROOT_TTREE", form::technology::ROOT_TTREE},
    {"ROOT_RNTUPLE", form::technology::ROOT_RNTUPLE},
    {"HDF5", form::technology::HDF5}};

  auto it = tech_lookup.find(tech_string);
  if (it == tech_lookup.end()) {
    throw std::runtime_error("Unknown technology: " + tech_string);
  }
  int const technology = it->second;

  form::experimental::config::ItemConfig input_cfg;
  form::experimental::config::tech_setting_config tech_cfg;
  for (auto const& name : products) {
    input_cfg.addItem(name, input_file, technology);
  }

  // Register the source object with Phlex
  s.source<FormInputSource>(
    module_label, input_cfg, tech_cfg, actual_creator, advertised_creator, products);

  std::cout << "FORM input source registered successfully\n";
}
