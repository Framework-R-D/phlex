// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_STORAGE_WRITE_ASSOCIATION_HPP
#define FORM_STORAGE_STORAGE_WRITE_ASSOCIATION_HPP

#include "storage_write_container.hpp"

#include <memory>

namespace form::detail::experimental {

  class storage_write_association : public storage_write_container {
  public:
    explicit storage_write_association(std::string const& name);
    ~storage_write_association() override = default;

    void set_attribute(std::string const& key, std::string const& value) override;
  };

} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_WRITE_ASSOCIATION_HPP
