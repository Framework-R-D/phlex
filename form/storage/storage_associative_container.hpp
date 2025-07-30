// Copyright (C) 2025 ...

#ifndef __STORAGE_ASSOCIATIVE_CONTAINER_H__
#define __STORAGE_ASSOCIATIVE_CONTAINER_H__

#include "storage_container.hpp"

#include <memory>

namespace form::detail::experimental {

  class Storage_Associative_Container : public Storage_Container {
  public:
    Storage_Associative_Container(const std::string& name);
    ~Storage_Associative_Container() = default;

    const std::string& top_name() { return m_tName; }
    const std::string& col_name() { return m_cName; }

    virtual void setParent(std::shared_ptr<IStorage_Container> parent) { m_parent = parent; }

  private:
    std::string m_tName;
    std::string m_cName;

    std::shared_ptr<IStorage_Container> m_parent;
  };

} // namespace form::detail::experimental

#endif
