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
                                   product_with_name& pb)
  {

    auto it = m_product_to_config.find(pb.label);
    if (it == m_product_to_config.end()) {
      throw std::runtime_error("No configuration found for product: " + pb.label);
    }

    m_pers_reader->read(creator, pb.label, segment_id, &pb.data, *pb.type);
  }
}
