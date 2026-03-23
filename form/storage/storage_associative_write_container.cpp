// Copyright (C) 2025 ...

#include "storage_associative_write_container.hpp"

using namespace form::detail::experimental;

Storage_Associative_Write_Container::Storage_Associative_Write_Container(std::string const& name) :
  Storage_Write_Container::Storage_Write_Container(name), m_tName(), m_cName(), m_parent(nullptr)
{
  auto del_pos = name.find("/");
  if (del_pos != std::string::npos) {
    m_tName = name.substr(0, del_pos);
    m_cName = name.substr(del_pos + 1);
  } else {
    m_tName = name;
    m_cName = "Main";
  }
}

Storage_Associative_Write_Container::~Storage_Associative_Write_Container() = default;

std::string const& Storage_Associative_Write_Container::top_name() { return m_tName; }

std::string const& Storage_Associative_Write_Container::col_name() { return m_cName; }

void Storage_Associative_Write_Container::setParent(
  std::shared_ptr<IStorage_Write_Container> parent)
{
  m_parent = parent;
}
