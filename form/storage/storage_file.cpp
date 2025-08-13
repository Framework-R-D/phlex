// Copyright (C) 2025 ...

#include "storage_file.hpp"

using namespace form::detail::experimental;

Storage_File::Storage_File(const std::string& name, char mode) : m_name(name), m_mode(mode) {}

const std::string& Storage_File::name() { return m_name; }

const char Storage_File::mode() { return m_mode; }
