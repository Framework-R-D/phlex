// Copyright (C) 2025 ...

#include "form.hpp"

namespace form::experimental {

  // Accept and store config
  form_interface::form_interface(std::shared_ptr<mock_phlex::product_type_names> tm,
                                 mock_phlex::config::parse_config const& config) :
    m_pers(nullptr), m_type_map(tm)
  {
    // Convert phlex config to form config
    form::experimental::config::output_item_config output_items;
    for (auto const& phlex_item : config.getItems()) {
      output_items.addItem(phlex_item.product_name, phlex_item.file_name, phlex_item.technology);
      m_product_to_config.emplace(
        phlex_item.product_name,
        form::experimental::config::PersistenceItem(
          phlex_item.product_name, phlex_item.file_name, phlex_item.technology));
    }

    config::tech_setting_config tech_config_settings;
    tech_config_settings.file_settings = config.getFileSettings();
    tech_config_settings.container_settings = config.getContainerSettings();

    m_pers = form::detail::experimental::createPersistence();
    m_pers->configureOutputItems(output_items);
    m_pers->configureTechSettings(tech_config_settings);
  }

  void form_interface::write(std::string const& creator, mock_phlex::product_base const& pb)
  {
    // Look up creator from PersistenceItem.
    auto it = m_product_to_config.find(pb.label);
    if (it == m_product_to_config.end()) {
      throw std::runtime_error("No configuration found for product: " + pb.label);
    }

    std::string const type = m_type_map->names[pb.type];
    // FIXME: Really only needed on first call
    std::map<std::string, std::string> products = {{pb.label, type}};
    m_pers->createContainers(creator, products);
    m_pers->registerWrite(creator, pb.label, pb.data, type);
    m_pers->commitOutput(creator, pb.id);
  }

  // Look up creator from config
  void form_interface::write(std::string const& creator,
                             std::vector<mock_phlex::product_base> const& batch)
  {
    if (batch.empty())
      return;

    // Look up creator from config based on product name. O(1) lookup instead of loop
    auto it = m_product_to_config.find(batch[0].label);
    if (it == m_product_to_config.end()) {
      throw std::runtime_error("No configuration found for product: " + batch[0].label);
    }

    // FIXME: Really only needed on first call
    std::map<std::string, std::string> products;
    for (auto const& pb : batch) {
      std::string const& type = m_type_map->names[pb.type];
      products.insert(std::make_pair(pb.label, type));
    }
    m_pers->createContainers(creator, products);
    for (auto const& pb : batch) {
      std::string const& type = m_type_map->names[pb.type];
      // FIXME: We could consider checking id to be identical for all product bases here
      m_pers->registerWrite(creator, pb.label, pb.data, type);
    }
    // Single commit per segment (product ID shared among products in the same segment)
    std::string const& id = batch[0].id;
    m_pers->commitOutput(creator, id);
  }

  void form_interface::read(std::string const& creator, mock_phlex::product_base& pb)
  {
    // Look up creator from config based on product name. O(1) lookup instead of loop
    auto it = m_product_to_config.find(pb.label);
    if (it == m_product_to_config.end()) {
      throw std::runtime_error("No configuration found for product: " + pb.label);
    }

    // Original type lookup
    std::string type = m_type_map->names[pb.type];

    // Use full_label instead of pb.label
    m_pers->read(creator, pb.label, pb.id, &pb.data, type);
  }
}
