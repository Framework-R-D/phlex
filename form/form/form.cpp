// Copyright (C) 2025 ...

#include "form.hpp"

namespace form::experimental {
  form_interface::form_interface(std::shared_ptr<phlex::product_type_names> tm) :
    m_pers(nullptr), m_type_map(tm)
  {
    m_pers = form::detail::experimental::createPersistence();
  }

  void form_interface::write(const std::string& creator, const phlex::product_base& pb)
  {
    const std::string type = m_type_map->names[pb.type];
    // FIXME: Really only needed on first call
    std::map<std::string, std::string> products = {{pb.label, type}};
    m_pers->createContainers(creator, products);

    m_pers->registerWrite(creator + "/" + pb.label, pb.data, type);

    m_pers->commitOutput(creator, pb.id);
  }

  void form_interface::write(const std::string& creator,
                             const std::vector<phlex::product_base>& batch)
  {
    if (batch.empty())
      return;

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
      m_pers->registerWrite(creator + "/" + pb.label, pb.data, type);
    }

    // Single commit per segment (product ID shared among products in the same segment)
    const std::string& id = batch[0].id;
    m_pers->commitOutput(creator, id);
  }

  void form_interface::read(phlex::product_base& pb)
  {
    // For now reading into 'empty' product_base, may change
    std::string type = m_type_map->names[pb.type];
    m_pers->read(pb.label, pb.id, &pb.data, type);
  }
}
