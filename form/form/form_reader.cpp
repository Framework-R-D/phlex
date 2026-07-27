// Copyright (C) 2025 ...

#include "form_reader.hpp"

#include <stdexcept>
#include <typeinfo>

namespace form::experimental {

  form_reader_interface::form_reader_interface(config::ItemConfig const& config_item,
                                               config::tech_setting_config const& tech_config) :
    m_pers_reader(nullptr)
  {
    for (auto const& item : config_item.getItems()) {
      m_product_to_config.emplace(item.product_name,
                                  form::experimental::config::PersistenceItem(
                                    item.product_name, item.file_name, item.technology));
    }

    m_pers_reader = form::detail::experimental::createPersistenceReader();
    m_pers_reader->configure(config_item);
    m_pers_reader->configureTechSettings(tech_config);
  }

  void form_reader_interface::read(std::string const& creator,
                                   std::string const& segment_id,
                                   product_with_name& product)
  {

    auto it = m_product_to_config.find(product.label);
    if (it == m_product_to_config.end()) {
      throw std::runtime_error("No configuration found for product: " + product.label);
    }

    m_pers_reader->read(creator, product.label, segment_id, &product.data, *product.type);
  }

  void form_reader_interface::prime(std::string const& creator,
                                    std::string const& product_name,
                                    std::type_info const& type)
  {
    m_pers_reader->prime(creator, product_name, type);
  }

  std::vector<std::string> form_reader_interface::indices(std::string const& creator,
                                                          std::string const& product_name)
  {
    return m_pers_reader->listIndices(creator, product_name);
  }
}
