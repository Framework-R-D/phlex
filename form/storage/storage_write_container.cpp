// Copyright (C) 2025 ...

#include "storage_write_container.hpp"

#include "storage_file.hpp"
#include <utility>

using namespace form::detail::experimental;

storage_write_container::storage_write_container(std::string name) :
  name_(std::move(name)), file_(nullptr)
{
}

std::string const& storage_write_container::name() { return name_; }

void storage_write_container::set_file(std::shared_ptr<i_storage_file> file) { file_ = file; }

void storage_write_container::setup_write(std::type_info const& /* type*/) {}

std::uint64_t storage_write_container::fill(void const* /* data*/) { return invalid_row_id; }

void storage_write_container::commit() {}

void storage_write_container::set_attribute(std::string const& /*name*/,
                                            std::string const& /*value*/)
{
  throw std::runtime_error(
    "storage_write_container::set_attribute does not accept any attributes for a container named " +
    name_);
}
