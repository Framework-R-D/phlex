// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_STORAGE_ASSOCIATIVE_CONTAINER_HPP
#define FORM_STORAGE_STORAGE_ASSOCIATIVE_CONTAINER_HPP

#include "storage_container.hpp"

#include <memory>

namespace form::detail::experimental {

  class Storage_Associative_Container : public Storage_Container {
  public:
    Storage_Associative_Container(std::string const& name);
    ~Storage_Associative_Container();

    std::string const& top_name();
    std::string const& col_name();

    virtual void setParent(std::shared_ptr<IStorage_Container> parent);

  private:
    std::string m_tName;
    std::string m_cName;

    std::shared_ptr<IStorage_Container> m_parent;
  };

} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_ASSOCIATIVE_CONTAINER_HPP
