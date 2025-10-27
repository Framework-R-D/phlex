// Copyright (C) 2025 ...

#include "storage_file.hpp"
#include <stdexcept>
#include <string>
#include <utility>

using namespace form::detail::experimental;

Storage_File::Storage_File(std::string  name, char mode) : m_name(std::move(name)), m_mode(mode) {}

auto Storage_File::name() -> std::string const& { return m_name; }

auto Storage_File::mode() -> char const { return m_mode; }

void Storage_File::setAttribute(std::string const& /*name*/, std::string const& /*value*/)
{
  throw std::runtime_error(
    "StorageFile::setAttribute does not accept any attributes for a file named " + m_name);
}
