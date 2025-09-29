// Copyright (C) 2025 ...

#include "storage_file.hpp"

using namespace form::detail::experimental;

Storage_File::Storage_File(std::string const& name, char mode) : m_name(name), m_mode(mode) {}

std::string const& Storage_File::name() { return m_name; }

char const Storage_File::mode() { return m_mode; }

void Storage_File::setAttribute(std::string const& /*name*/, std::string const& /*value*/)
{
  throw std::runtime_error(
    "StorageFile::setAttribute does not accept any attributes for a file named " + m_name);
}
