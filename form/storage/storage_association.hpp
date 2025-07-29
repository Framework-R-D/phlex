// Copyright (C) 2025 ...

#ifndef __STORAGE_ASSOCIATION_H__
#define __STORAGE_ASSOCIATION_H__

#include "storage_container.hpp"

#include <memory>

namespace form::detail::experimental {

  class Storage_Association : public Storage_Container {
  public:
    Storage_Association(const std::string& name);
    ~Storage_Association() = default;
  };

} // namespace form::detail::experimental

#endif
