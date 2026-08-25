// Copyright (C) 2025 ...

#include "storage_file.hpp"

#include <utility>

using namespace form::detail::experimental;

storage_file::storage_file(std::string name, char mode) : name_(std::move(name)), mode_(mode) {}

std::string const& storage_file::name() { return name_; }

char storage_file::mode() { return mode_; }

void storage_file::set_attribute(std::string const& /*name*/, std::string const& /*value*/)
{
  throw std::runtime_error(
    "storage_file::set_attribute does not accept any attributes for a file named " + name_);
}
