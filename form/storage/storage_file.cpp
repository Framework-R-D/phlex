// Copyright (C) 2025 ...

#include "storage_file.hpp"

#include "descriptor.hpp"

#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace form::detail::experimental;

storage_file::storage_file(std::string name, char mode) :
  name_(std::move(name)), mode_(mode), descriptor_(std::make_unique<memory_descriptor>(name_))
{
}

std::string const& storage_file::name() { return name_; }

char storage_file::mode() { return mode_; }

void storage_file::set_attribute(std::string const& /*name*/, std::string const& /*value*/)
{
  error(
    "does not accept any attributes for a file named " + name_);
}

void storage_file::set_descriptor(std::unique_ptr<descriptor> descriptor)
{
  descriptor_ = std::move(descriptor);
}

void storage_file::write_descriptor(std::vector<std::pair<std::string, std::string>> const& entries)
{
  descriptor_->write(entries);
}

std::optional<std::vector<std::pair<std::string, std::string>>> storage_file::read_descriptor()
{
  return descriptor_->read();
}
