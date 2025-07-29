// Copyright (C) 2025 ...

#include "storage_associative_container.hpp"

using namespace form::detail::experimental;

Storage_Associative_Container::Storage_Associative_Container(const std::string& name) :
  Storage_Container::Storage_Container(name), m_tName(), m_cName(), m_parent(nullptr)
{
  auto del_pos = m_name.find("/");
  if (del_pos != std::string::npos) {
    m_tName = m_name.substr(0, del_pos);
    m_cName = m_name.substr(del_pos + 1);
  } else {
    m_tName = m_name;
    m_cName = "Main";
  }
}
