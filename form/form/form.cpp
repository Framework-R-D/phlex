// Copyright (C) 2025 ...

#include "form.hpp"

namespace form::experimental {

  // Accept and store config
  form_interface::form_interface(std::shared_ptr<mock_phlex::product_type_names> tm,
                                 mock_phlex::config::parse_config const& phlex_config) :
    m_pers(nullptr), m_type_map(tm), m_config()
  {
    // Convert phlex config to form config
    for (auto const& phlex_item : phlex_config.getItems()) {
      m_config.addItem(phlex_item.product_name, phlex_item.file_name, phlex_item.technology);
    }
    m_pers = form::detail::experimental::createPersistence(m_config);
  }

  void form_interface::write(std::string const& creator, mock_phlex::product_base const& pb)
  {

    std::string const type = m_type_map->names[pb.type];
    // FIXME: Really only needed on first call
    std::map<std::string, std::string> products = {{pb.label, type}};
    m_pers->createContainers(creator, products);
    m_pers->registerWrite(creator, pb.label, pb.data, type);
    m_pers->commitOutput(creator, pb.id);
  }

  void form_interface::write(std::string const& creator,
                             std::vector<mock_phlex::product_base> const& batch)
  {
    if (batch.empty())
      return;

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
    // Original type lookup
    std::string type = m_type_map->names[pb.type];

    // Use full_label instead of pb.label
    m_pers->read(creator, pb.label, pb.id, &pb.data, type);
  }
}
