// Copyright (C) 2025 ...

#include "storage_read_container.hpp"
#include "storage_file.hpp"

using namespace form::detail::experimental;

Storage_Read_Container::Storage_Read_Container(std::string const& name) :
  m_name(name), m_file(nullptr)
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

std::string const& Storage_Read_Container::name() { return m_name; }

std::string const& Storage_Read_Container::top_name() { return m_tName; }

std::string const& Storage_Read_Container::col_name() { return m_cName; }

void Storage_Read_Container::setFile(std::shared_ptr<IStorage_File> file) { m_file = file; }

bool Storage_Read_Container::read(int /* id*/,
                                  void const** /*data*/,
                                  std::type_info const& /* type*/)
{
  return false;
}

void Storage_Read_Container::setAttribute(std::string const& /*name*/, std::string const& /*value*/)
{
  throw std::runtime_error(
    "Storage_Read_Container::setAttribute does not accept any attributes for a container named " +
    m_name);
}
