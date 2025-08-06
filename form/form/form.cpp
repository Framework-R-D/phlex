// Copyright (C) 2025 ...

#include "form.hpp"

namespace form::experimental {

  // Accept and store config
  form_interface::form_interface(std::shared_ptr<phlex::product_type_names> tm,
                                 const phlex::config::parse_config& config) :
    m_pers(nullptr), m_type_map(tm), m_config()
  {
    // Convert phlex config to form config
    for (const auto& phlex_item : config.getItems()) {
      m_config.addItem(phlex_item.product_name, phlex_item.file_name, phlex_item.technology);

      // Build lookup maps for O(1) access
      m_product_to_config.emplace(
        phlex_item.product_name,
        form::experimental::config::PersistenceItem(
          phlex_item.product_name, phlex_item.file_name, phlex_item.technology));
    }
    m_pers = form::detail::experimental::createPersistence(m_config);
  }

  void form_interface::write(const std::string& creator, const phlex::product_base& pb)
  {
    // Look up creator from PersistenceItem. O(1) lookup instead of loop
    auto it = m_product_to_config.find(pb.label);
    if (it == m_product_to_config.end()) {
      throw std::runtime_error("No configuration found for product: " + pb.label);
    }

    const std::string type = m_type_map->names[pb.type];
    // FIXME: Really only needed on first call
    std::map<std::string, std::string> products = {{pb.label, type}};
    m_pers->createContainers(creator, products);
    m_pers->registerWrite(creator, pb.label, pb.data, type);
    m_pers->commitOutput(creator, pb.id);
  }

  // Look up creator from config
  void form_interface::write(const std::string& creator,
                             const std::vector<phlex::product_base>& batch)
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
    for (const auto& pb : batch) {
      const std::string& type = m_type_map->names[pb.type];
      products.insert(std::make_pair(pb.label, type));
    }
    m_pers->createContainers(creator, products);
    for (const auto& pb : batch) {
      const std::string& type = m_type_map->names[pb.type];
      // FIXME: We could consider checking id to be identical for all product bases here
      m_pers->registerWrite(creator, pb.label, pb.data, type);
    }
    // Single commit per segment (product ID shared among products in the same segment)
    const std::string& id = batch[0].id;
    m_pers->commitOutput(creator, id);
  }

  void form_interface::read(const std::string& creator, phlex::product_base& pb)
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
