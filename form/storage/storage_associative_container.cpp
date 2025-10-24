// Copyright (C) 2025 ...

#include "storage_associative_container.hpp"
#include "storage/istorage.hpp"
#include "storage/storage_container.hpp"
#include <memory>
#include <string>

using namespace form::detail::experimental;

Storage_Associative_Container::Storage_Associative_Container(std::string const& name) :
  Storage_Container::Storage_Container(name), m_tName(), m_cName(), m_parent(nullptr)
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

Storage_Associative_Container::~Storage_Associative_Container() = default;

auto Storage_Associative_Container::top_name() -> std::string const& { return m_tName; }

auto Storage_Associative_Container::col_name() -> std::string const& { return m_cName; }

void Storage_Associative_Container::setParent(std::shared_ptr<IStorage_Container> parent)
{
  m_parent = parent;
}
