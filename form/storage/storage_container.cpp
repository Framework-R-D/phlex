// Copyright (C) 2025 ...

#include "storage_container.hpp"
#include "storage/istorage.hpp"
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

using namespace form::detail::experimental;

Storage_Container::Storage_Container(std::string  name) : m_name(std::move(name)), m_file(nullptr) {}

auto Storage_Container::name() -> std::string const& { return m_name; }

void Storage_Container::setFile(std::shared_ptr<IStorage_File> file) { m_file = file; }

void Storage_Container::setupWrite(std::string const& /* type*/) { return; }

void Storage_Container::fill(void const* /* data*/) { return; }

void Storage_Container::commit() { return; }

auto Storage_Container::read(int /* id*/, void const** /*data*/, std::string& /* type*/) -> bool
{
  return false;
}

void Storage_Container::setAttribute(std::string const& /*name*/, std::string const& /*value*/)
{
  throw std::runtime_error(
    "Storage_Container::setAttribute does not accept any attributes for a container named " +
    m_name);
}
