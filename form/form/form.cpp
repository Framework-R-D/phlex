// Copyright (C) 2025 ...

#include "form.hpp"

namespace form::experimental {
  form_interface::form_interface(std::shared_ptr<phlex::product_type_names> tm) :
    m_pers(nullptr), m_type_map(tm)
  {
    m_pers = createPersistence();
  }

  void form_interface::write(const phlex::product_base& pb)
  {
    // Prepare for having phlex algorithms return several products, do registerWrite() for each
    const std::string type = m_type_map->names[pb.type];
    m_pers->registerWrite(pb.label, pb.data, type);
    m_pers->commitOutput(pb.label, pb.id);
  }

  void form_interface::read(phlex::product_base& pb)
  {
    // For now reading into 'empty' product_base, may change
    std::string type = m_type_map->names[pb.type];
    m_pers->read(pb.label, pb.id, &pb.data, type);
  }
}
