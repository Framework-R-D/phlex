// Copyright (C) 2025 ...

#include "form.hpp"

namespace form::experimental {
  form_interface::form_interface() : m_pers(nullptr) { m_pers = createPersistence(); }

  void form_interface::write(const phlex::product_base& pb, const std::string& type)
  {
    // Prepare for having phlex algorithms return several products, do registerWrite() for each
    m_pers->registerWrite(pb.label, pb.data, type);
    m_pers->commitOutput(pb.label, pb.id);
  }

  void form_interface::read(phlex::product_base& pb, std::string& type)
  { // For now reading into 'empty' product_base, may change
    m_pers->read(pb.label, pb.id, &pb.data, type);
  }
}
