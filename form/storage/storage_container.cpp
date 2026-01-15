// Copyright (C) 2025 ...

#include "storage_container.hpp"
#include "storage_file.hpp"

#include <stdexcept>

using namespace form::detail::experimental;

Storage_Container::Storage_Container(std::string const& name) : m_name(name), m_file(nullptr) {}

std::string const& Storage_Container::name() { return m_name; }

void Storage_Container::setFile(std::shared_ptr<IStorage_File> file) { m_file = file; }

void Storage_Container::setupWrite(std::type_info const& /*type*/) { return; }

void Storage_Container::fill(void const* /* data*/) { return; }

void Storage_Container::commit() { return; }

bool Storage_Container::read(int /* id*/, void const** /*data*/, std::type_info const& /* type*/)
{
  return false;
}

void Storage_Container::setAttribute(std::string const& /*name*/, std::string const& /*value*/)
{
  throw std::runtime_error(
    "Storage_Container::setAttribute does not accept any attributes for a container named " +
    m_name);
}
