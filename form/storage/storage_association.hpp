// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_STORAGE_ASSOCIATION_HPP
#define FORM_STORAGE_STORAGE_ASSOCIATION_HPP

#include "storage_container.hpp"

#include <memory>

namespace form::detail::experimental {

  class Storage_Association : public Storage_Container {
  public:
    Storage_Association(std::string const& name);
    ~Storage_Association() = default;

    void setAttribute(std::string const& key, std::string const& value) override;
  };

} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_ASSOCIATION_HPP
