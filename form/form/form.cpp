// Copyright (C) 2025 ...

#include "form.hpp"

#include <stdexcept>

namespace form::experimental {

  form_interface::form_interface(std::shared_ptr<product_type_names> tm,
                                 config::output_item_config const& output_config,
                                 config::tech_setting_config const& tech_config) :
    m_pers(nullptr), m_type_map(tm)
  {
    for (auto const& item : output_config.getItems()) {
      m_product_to_config.emplace(item.product_name,
                                  form::experimental::config::PersistenceItem(
                                    item.product_name, item.file_name, item.technology));
    }

    m_pers = form::detail::experimental::createPersistence();
    m_pers->configureOutputItems(output_config);
    m_pers->configureTechSettings(tech_config);
  }

  void form_interface::write(std::string const& creator,
                             std::string const& segment_id,
                             product_with_name const& pb)
  {

    auto it = m_product_to_config.find(pb.label);
    if (it == m_product_to_config.end()) {
      throw std::runtime_error("No configuration found for product: " + pb.label);
    }

    std::string const type = m_type_map->names[pb.type];

    std::map<std::string, std::string> products = {{pb.label, type}};
    m_pers->createContainers(creator, products);

    m_pers->registerWrite(creator, pb.label, pb.data, type);

    m_pers->commitOutput(creator, segment_id);
  }

  void form_interface::write(std::string const& creator,
                             std::string const& segment_id,
                             std::vector<product_with_name> const& products)
  {

    if (products.empty())
      return;

    auto it = m_product_to_config.find(products[0].label);
    if (it == m_product_to_config.end()) {
      throw std::runtime_error("No configuration found for product: " + products[0].label);
    }

    // FIXME: Really only needed on first call
    std::map<std::string, std::string> product_types;
    for (auto const& pb : products) {
      std::string const& type = m_type_map->names[pb.type];
      product_types.insert(std::make_pair(pb.label, type));
    }

    m_pers->createContainers(creator, product_types);

    for (auto const& pb : products) {
      std::string const& type = m_type_map->names[pb.type];
      // FIXME: We could consider checking id to be identical for all product bases here
      m_pers->registerWrite(creator, pb.label, pb.data, type);
    }

    m_pers->commitOutput(creator, segment_id);
  }

  void form_interface::read(std::string const& creator,
                            std::string const& segment_id,
                            product_with_name& pb)
  {

    auto it = m_product_to_config.find(pb.label);
    if (it == m_product_to_config.end()) {
      throw std::runtime_error("No configuration found for product: " + pb.label);
    }

    std::string type = m_type_map->names[pb.type];

    m_pers->read(creator, pb.label, segment_id, &pb.data, type);
  }
}
