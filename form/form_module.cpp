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
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

  form::detail::experimental::cell_index to_cell_index(phlex::data_cell_index const& index)
  {
    form::detail::experimental::cell_index cell;
    cell.id = index.to_string();

    // The job root has no parent and is not a data layer.
    for (auto const* node = &index; node->has_parent(); node = node->parent().get()) {
      cell.layer_names.push_back(node->layer_name().trans_get_string());
      cell.layer_values.push_back(static_cast<std::uint64_t>(node->number()));
    }
    // Reverse to outermost-first layer order.
    std::ranges::reverse(cell.layer_names);
    std::ranges::reverse(cell.layer_values);

    // Generate names for unnamed layers.
    for (std::size_t i = 0; i < cell.layer_names.size(); ++i) {
      if (cell.layer_names[i].empty()) {
        cell.layer_names[i] = form::detail::experimental::unnamed_layer_name(i);
      }
    }
    return cell;
  }

  class form_output_module {
  public:
    form_output_module(std::string output_file,
                       form::technology::id technology,
                       std::vector<std::string> const& products_to_save) :
      output_file_(std::move(output_file)), technology_(technology)
    {
      std::cout << "form_output_module initialized\n";
      std::cout << "  Output file: " << output_file_ << "\n";
      std::cout << "  Technology: " << form::technology::to_string(technology_) << "\n";

      // Build FORM configuration
      form::experimental::config::item_config output_cfg;
      form::experimental::config::tech_setting_config tech_cfg;

      // FIXME: Temporary solution to accommodate Phlex limitation.
      // Eventually, Phlex will communicate to FORM which products will be written
      // before executing any algorithms

      // Temp. Sol for Phlex Prototype 0.1
      // Register products from config
      for (auto const& product : products_to_save) {
        output_cfg.add_item(product, output_file_, technology_);
      }

      // Initialize FORM interface
      form_interface_ =
        std::make_unique<form::experimental::form_writer_interface>(output_cfg, tech_cfg);
    }

    ~form_output_module()
    {
      // Phlex has no end-of-job hook, so finalize while the interface is alive.
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

      // Extract creator (algorithm name)
      auto const& creator = store.source();

      auto const cell = to_cell_index(*store.index());

      std::cout << "\n=== form_output_module::save_data_products ===\n";
      std::cout << "Creator: " << creator.to_string() << "\n";
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
      form_interface_->write(creator.to_string(), cell, products);
      std::cout << "Wrote " << products.size() << " products to FORM\n";
    }

  private:
    // Algorithm configuration fixed at construction; intentionally immutable for object lifetime.
    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
    std::string const output_file_;
    form::technology::id const technology_;
    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
    std::unique_ptr<form::experimental::form_writer_interface> form_interface_;
  };

}

PHLEX_REGISTER_ALGORITHMS(m, config)
{
  std::cout << "Registering FORM output module...\n";

  // Extract configuration from Phlex config
  auto const output_file = config.get<std::string>("output_file", "output.root");
  auto const tech_string = config.get<std::string>("technology", "ROOT_TTREE");

  std::cout << "Configuration:\n";
  std::cout << "  output_file: " << output_file << "\n";
  std::cout << "  technology: " << tech_string << "\n";

  auto const technology = form::technology::from_string(tech_string);

  auto products_to_save = config.get<std::vector<std::string>>("products");

  // Phlex needs an OBJECT
  // Create the FORM output module
  auto form_output = m.make<form_output_module>(output_file, technology, products_to_save);

  // Phlex needs a MEMBER FUNCTION to call
  // Register the callback that Phlex will invoke
  form_output.output("save_data_products", &form_output_module::save_data_products);

  std::cout << "FORM output module registered successfully\n";
}
