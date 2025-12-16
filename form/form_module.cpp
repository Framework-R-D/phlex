#include "phlex/model/product_store.hpp"
#include "phlex/model/products.hpp"
#include "phlex/module.hpp"

// FORM headers - these need to be available via CMake configuration
// need to set up the build system to find these headers
#include "form/config.hpp"
#include "form/form.hpp"
#include "form/technology.hpp"

#include <iostream>

namespace {

  class FormOutputModule {
  public:
    FormOutputModule(std::shared_ptr<form::experimental::product_type_names> type_map,
                     std::string output_file,
                     int technology) :
      m_type_map(type_map), m_output_file(std::move(output_file)), m_technology(technology)
    {
      std::cout << "FormOutputModule initialized\n";
      std::cout << "  Output file: " << m_output_file << "\n";
      std::cout << "  Technology: " << m_technology << "\n";

      // Build FORM configuration
      form::experimental::config::output_item_config output_cfg;
      form::experimental::config::tech_setting_config tech_cfg;

      // Register products that will be written
      // Register "sum" product with output file and technology
      output_cfg.addItem("sum", m_output_file, m_technology);
      
      std::cout << "  Registered product 'sum' with FORM\n";

      // Initialize FORM interface
      m_form_interface =
        std::make_unique<form::experimental::form_interface>(type_map, output_cfg, tech_cfg);
    }

    // This method is called by Phlex - signature must be: void(product_store const&)
    void save_data_products(phlex::experimental::product_store const& store)
    {
      // Check if store is empty - smart way, check store not products vector
      if (store.empty()) {
        return;
      }

      // STEP 1: Extract metadata from Phlex's product_store

      // Extract creator (algorithm name)
      std::string creator = store.source();

      // Extract segment ID (partition) - extract once for entire store
      std::string segment_id = store.id()->to_string();

      std::cout << "\n=== FormOutputModule::save_data_products ===\n";
      std::cout << "Creator: " << creator << "\n";
      std::cout << "Segment ID: " << segment_id << "\n";
      std::cout << "Number of products: " << store.size() << "\n";

      // STEP 2: Convert each Phlex product to FORM format

      // Collect all products for writing
      std::vector<form::experimental::product_with_name> products;

      // Reserve space for efficiency - avoid reallocations
      products.reserve(store.size());

      // Iterate through all products in the store
      for (auto const& [product_name, product_ptr] : store) {
        // product_name: "tracks" (from the map key)
        // product_ptr: pointer to the actual product data

        std::cout << "  Product: " << product_name << "\n";

        // Create FORM product with metadata
        products.emplace_back(product_name,           // label, from map key
                              product_ptr->address(), // data,  from phlex product_base
                              product_ptr->type()     // type,  from phlex product_base
        );
      }

      // STEP 3: Send everything to FORM for persistence

      // Write all products to FORM
      // Pass segment_id once for entire collection (not duplicated in each product)
      // No need to check if products is empty - already checked store.empty() above
      m_form_interface->write(creator, segment_id, products);
      std::cout << "Wrote " << products.size() << " products to FORM\n";
    }

  private:
    std::shared_ptr<form::experimental::product_type_names> m_type_map;
    std::string m_output_file;
    int m_technology;
    std::unique_ptr<form::experimental::form_interface> m_form_interface;
  };

}

PHLEX_EXPERIMENTAL_REGISTER_ALGORITHMS(m, config)
{
  std::cout << "Registering FORM output module...\n";

  // Create type map
  auto type_map = std::make_shared<form::experimental::product_type_names>();

  // TODO: Register product types
  // This needs to be populated with actual types your application uses
  // Example:
  // type_map->names[std::type_index(typeid(TrackData))] = "TrackData";
  // type_map->names[std::type_index(typeid(HitCollection))] = "HitCollection";

  // Extract configuration from Phlex config
  std::string output_file = config.get<std::string>("output_file", "output.root");
  std::string tech_string = config.get<std::string>("technology", "ROOT_TTREE");

  std::cout << "Configuration:\n";
  std::cout << "  output_file: " << output_file << "\n";
  std::cout << "  technology: " << tech_string << "\n";

  // Map Phlex config string to FORM technology constant
  int technology = form::technology::ROOT_TTREE; // default

  if (tech_string == "ROOT_TTREE") {
    technology = form::technology::ROOT_TTREE;
  } else if (tech_string == "ROOT_RNTUPLE") {
    technology = form::technology::ROOT_RNTUPLE;
  } else if (tech_string == "HDF5") {
    technology = form::technology::HDF5;
  } else {
    throw std::runtime_error("Unknown technology: " + tech_string);
  }

  // Phlex needs an OBJECT
  // Create the FORM output module
  auto form_output = m.make<FormOutputModule>(type_map, output_file, technology);

  // Phlex needs a MEMBER FUNCTION to call
  // Register the callback that Phlex will invoke
  form_output.output("save_data_products", &FormOutputModule::save_data_products);

  std::cout << "FORM output module registered successfully\n";
}
