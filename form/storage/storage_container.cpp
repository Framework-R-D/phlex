// Copyright (C) 2025 ...

#include "storage_container.hpp"
#include "storage_file.hpp"

using namespace form::detail::experimental;
Storage_Container::Storage_Container(const std::string& name) :
  m_name(name), m_file(nullptr), m_tName(""), m_cName("")
{
  auto del_pos = m_name.find("/"); // For associative container
  if (del_pos != std::string::npos) {
    m_tName = m_name.substr(0, del_pos);
    m_cName = m_name.substr(del_pos + 1);
  } else {
    m_tName = m_name;
    m_cName = "Main";
  }
}

Storage_Container::~Storage_Container() {}

const std::string& Storage_Container::name() { return m_name; }

void Storage_Container::setFile(std::shared_ptr<IStorage_File> file) { m_file = file; }

void Storage_Container::fill(const void* /* data*/, const std::string& /* type*/) { return; }

bool Storage_Container::read(int /* id*/, const void** /*data*/, std::string& /* type*/)
{
  return false;
}
