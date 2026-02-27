// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_STORAGE_READ_ASSOCIATION_HPP
#define FORM_STORAGE_STORAGE_READ_ASSOCIATION_HPP

#include "storage_read_container.hpp"

#include <memory>

namespace form::detail::experimental {

  class Storage_Read_Association : public Storage_Read_Container {
  public:
    Storage_Read_Association(std::string const& name);
    ~Storage_Read_Association() = default;

    void setAttribute(std::string const& key, std::string const& value) override;
  };

} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_READ_ASSOCIATION_HPP
