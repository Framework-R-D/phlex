// Copyright (C) 2025 ...

#include "storage_association.hpp"

using namespace form::detail::experimental;

Storage_Association::Storage_Association(const std::string& name) :
  Storage_Container::Storage_Container(name)
{
  auto del_pos = m_name.find("/");
  if (del_pos != std::string::npos) {
    m_name = m_name.substr(0, del_pos);
  }
}
