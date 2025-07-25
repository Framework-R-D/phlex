// Copyright (C) 2025 ...

#include "form.hpp"

namespace form::experimental {
  form_interface::form_interface(std::shared_ptr<phlex::product_type_names> tm) :
    m_pers(nullptr), m_type_map(tm)
  {
    m_pers = form::detail::experimental::createPersistence();
  }

  void form_interface::write(const phlex::product_base& pb)
  {
    // Prepare for having phlex algorithms return several products, do registerWrite() for each
    const std::string type = m_type_map->names[pb.type];
    m_pers->registerWrite(pb.label, pb.data, type);
    m_pers->commitOutput(pb.label, pb.id);
  }

  void form_interface::write(const std::vector<phlex::product_base>& batch)
  {
    if (batch.empty())
      return;

    for (const auto& pb : batch) {
      const std::string& type = m_type_map->names[pb.type];
      // FIXME: We could consider checking id and creator to be identical for all product bases here
      m_pers->registerWrite(pb.label, pb.data, type);
    }

    // Single commit per segment (product ID shared among products in the same segment)
    const std::string& label = batch[0].label;
    const std::string& id = batch[0].id;
    m_pers->commitOutput(label, id);
  }

  void form_interface::read(phlex::product_base& pb)
  {
    // For now reading into 'empty' product_base, may change
    std::string type = m_type_map->names[pb.type];
    m_pers->read(pb.label, pb.id, &pb.data, type);
  }
}
