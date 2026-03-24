// Copyright (C) 2025 ...

#include "form_writer.hpp"

#include <stdexcept>
#include <typeinfo>

namespace form::experimental {

  form_writer_interface::form_writer_interface(config::output_item_config const& output_config,
                                               config::tech_setting_config const& tech_config) :
    m_pers_writer(nullptr)
  {
    for (auto const& item : output_config.getItems()) {
      m_product_to_config.emplace(item.product_name,
                                  form::experimental::config::PersistenceItem(
                                    item.product_name, item.file_name, item.technology));
    }

    m_pers_writer = form::detail::experimental::createPersistenceWriter();
    m_pers_writer->configureOutputItems(output_config);
    m_pers_writer->configureTechSettings(tech_config);
  }

  void form_writer_interface::write(std::string const& creator,
                                    std::string const& segment_id,
                                    product_with_name const& pb)
  {

    auto it = m_product_to_config.find(pb.label);
    if (it == m_product_to_config.end()) {
      throw std::runtime_error("No configuration found for product: " + pb.label);
    }

    std::map<std::string, std::type_info const*> products = {{pb.label, pb.type}};
    m_pers_writer->createContainers(creator, products);

    m_pers_writer->registerWrite(creator, pb.label, pb.data, *pb.type);

    m_pers_writer->commitOutput(creator, segment_id);
  }

  void form_writer_interface::write(std::string const& creator,
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
    std::map<std::string, std::type_info const*> product_types;
    for (auto const& pb : products) {
      product_types.insert(std::make_pair(pb.label, pb.type));
    }

    m_pers_writer->createContainers(creator, product_types);

    for (auto const& pb : products) {
      // FIXME: We could consider checking id to be identical for all product bases here
      m_pers_writer->registerWrite(creator, pb.label, pb.data, *pb.type);
    }

    m_pers_writer->commitOutput(creator, segment_id);
  }

}
