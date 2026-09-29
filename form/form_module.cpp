#include "phlex/configuration.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/model/products.hpp"
#include "phlex/module.hpp"

// FORM headers - these need to be available via CMake configuration
// need to set up the build system to find these headers
#include "core/cell_index.hpp"
#include "core/technology.hpp"
#include "form/config.hpp"
#include "form/form_writer.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

  form::detail::experimental::cell_index to_cell_index(phlex::data_cell_index const& index)
  {
    form::detail::experimental::cell_index cell;
    cell.id = index.to_string();

    // The job root has no parent and is not a data layer.
    for (auto const* node = &index; node->has_parent(); node = node->parent().get()) {
      cell.hierarchy.layer_names.push_back(node->layer_name().trans_get_string());
      cell.layer_values.push_back(static_cast<std::uint64_t>(node->number()));
    }
    // Reverse to outermost-first layer order.
    std::ranges::reverse(cell.hierarchy.layer_names);
    std::ranges::reverse(cell.layer_values);

    return cell;
  }

  // Add an output's products, rejecting duplicate (product, file, technology) entries.
  void add_output(form::experimental::config::item_config& output_cfg,
                  phlex::configuration const& output,
                  std::string const& output_name)
  {
    auto const output_file = output.get<std::string>("output_file", "output.root");
    auto const tech_string = output.get<std::string>("technology", "ROOT_TTREE");
    auto const technology = form::technology::from_string(tech_string);
    // FIXME: Phlex should provide the products to be written before the algorithm runs.
    auto const products = output.get<std::vector<std::string>>("products");

    std::cout << "  output '" << output_name << "': " << output_file << " (" << tech_string
              << ")\n";

    for (auto const& product : products) {
      auto const& items = output_cfg.get_items();
      if (std::ranges::any_of(items, [&](auto const& item) {
            return item.product_name == product && item.file_name == output_file &&
                   item.technology == technology;
          })) {
        throw std::runtime_error(
          std::format("form_module: product '{}' is configured more than once for output file "
                      "'{}' ({})",
                      product,
                      output_file,
                      tech_string));
      }
      output_cfg.add_item(product, output_file, technology);
    }
  }

  // Support single-output and nested multi-output configurations.
  form::experimental::config::item_config output_config(phlex::configuration const& config)
  {
    form::experimental::config::item_config output_cfg;

    auto const outputs = config.get_if_present<phlex::configuration>("outputs");
    if (!outputs) {
      add_output(output_cfg, config, "default");
      return output_cfg;
    }

    auto const keys = config.keys();
    for (auto const* key : {"output_file", "technology", "products"}) {
      if (std::ranges::contains(keys, key)) {
        throw std::runtime_error(std::format(
          "form_module: '{}' cannot be combined with 'outputs'; configure it per output", key));
      }
    }
    auto const names = outputs->keys();
    if (names.empty()) {
      throw std::runtime_error("form_module: 'outputs' must contain at least one output");
    }
    for (auto const& name : names) {
      auto const output = outputs->get<phlex::configuration>(name);
      // Nested outputs must specify their output file.
      if (!std::ranges::contains(output.keys(), "output_file")) {
        throw std::runtime_error(
          std::format("form_module: output '{}' must specify 'output_file'", name));
      }
      add_output(output_cfg, output, name);
    }
    return output_cfg;
  }

  class form_output_module {
  public:
    explicit form_output_module(form::experimental::config::item_config const& output_cfg)
    {
      form::experimental::config::tech_setting_config const tech_cfg;
      form_interface_ =
        std::make_unique<form::experimental::form_writer_interface>(output_cfg, tech_cfg);
    }

    ~form_output_module()
    {
      // Phlex currently has no end-of-job hook for output modules. Finalize here as the last
      // opportunity while the interface is alive. Failures during destruction can only be logged.
      // TODO: Move finalization to an end-of-job hook when one is available.
      try {
        form_interface_->finalize();
      } catch (std::exception const& e) {
        std::cerr << "form_output_module: finalize() failed: " << e.what() << '\n';
      } catch (...) {
        std::cerr << "form_output_module: finalize() failed with an unknown exception\n";
      }
    }

    form_output_module(form_output_module const&) = delete;
    form_output_module& operator=(form_output_module const&) = delete;
    form_output_module(form_output_module&&) = delete;
    form_output_module& operator=(form_output_module&&) = delete;

    // This method is called by Phlex - signature must be: void(product_store const&)
    void save_data_products(phlex::experimental::product_store const& store)
    {
      // Check if store is empty - smart way, check store not products vector
      if (store.empty()) {
        return;
      }

      // STEP 1: Extract metadata from Phlex's product_store

      // Extract the creator (algorithm name) and stage.
      auto const& creator = store.source();
      auto const& stage = store.stage().trans_get_string();

      auto const cell = to_cell_index(*store.index());

      std::cout << "\n=== form_output_module::save_data_products ===\n";
      std::cout << "Creator: " << creator.to_string() << "\n";
      std::cout << "Stage: " << stage << "\n";
      std::cout << "Data cell: " << cell.id << "\n";
      std::cout << "Number of products: " << store.size() << "\n";

      // STEP 2: Convert each Phlex product to FORM format

      // Collect all products for writing
      std::vector<form::experimental::product_with_name> products;

      // Reserve space for efficiency - avoid reallocations
      products.reserve(store.size());

      // Iterate through all products in the store
      for (auto const& [product_spec, product_ptr] : store) {
        // product_spec: "tracks" (from the map key)
        // product_ptr: pointer to the actual product data
        assert(product_ptr && "store should not contain null product_ptr");

        std::cout << "  Product: " << product_spec.to_string() << "\n";

        // Create FORM product with metadata
        products.emplace_back(product_spec.suffix().trans_get_string(), // label, from map key
                              product_ptr->address(), // data,  from phlex product_base
                              &product_ptr->type()    // type, from phlex product_base
        );
      }

      // STEP 3: Send everything to FORM for persistence

      // The cell is passed once for the entire product collection.
      form_interface_->write(creator.to_string(), stage, cell, products);
      std::cout << "Wrote " << products.size() << " products to FORM\n";
    }

  private:
    std::unique_ptr<form::experimental::form_writer_interface> form_interface_;
  };

}

PHLEX_REGISTER_ALGORITHMS(m, config)
{
  std::cout << "Registering FORM output module...\n";

  std::cout << "Configuration:\n";
  auto const output_cfg = output_config(config);

  // Phlex needs an OBJECT
  // Create the FORM output module
  auto form_output = m.make<form_output_module>(output_cfg);

  // Phlex needs a MEMBER FUNCTION to call
  // Register the callback that Phlex will invoke
  form_output.output("save_data_products", &form_output_module::save_data_products);

  std::cout << "FORM output module registered successfully\n";
}
