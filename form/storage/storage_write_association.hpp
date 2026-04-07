// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_STORAGE_WRITE_ASSOCIATION_HPP
#define FORM_STORAGE_STORAGE_WRITE_ASSOCIATION_HPP

#include "storage_write_container.hpp"

#include <memory>

namespace form::detail::experimental {

  class Storage_Write_Association : public Storage_Write_Container {
  public:
    Storage_Write_Association(std::string const& name);
    ~Storage_Write_Association() = default;

    void setAttribute(std::string const& key, std::string const& value) override;
  };

} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_WRITE_ASSOCIATION_HPP
