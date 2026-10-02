// Copyright (C) 2025 ...

#include "storage_read_container.hpp"

#include "core/container_naming.hpp"
#include "storage/istorage.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <typeinfo>

using namespace form::detail::experimental;

storage_read_container::storage_read_container(std::string const& name) :
  name_(name), t_name_(row_space_of(name)), c_name_(label_of(name).value_or("Main")), file_(nullptr)
{
}

std::string const& storage_read_container::name() { return name_; }

std::string const& storage_read_container::top_name() { return t_name_; }

std::string const& storage_read_container::col_name() { return c_name_; }

void storage_read_container::set_file(std::shared_ptr<i_storage_file> file) { file_ = file; }

void storage_read_container::prime(std::type_info const& /*type*/) {}

bool storage_read_container::read(int /* id*/,
                                  void const** /*data*/,
                                  std::type_info const& /* type*/)
{
  return false;
}

int storage_read_container::entries() { return 0; }

void storage_read_container::set_attribute(std::string const& /*name*/,
                                           std::string const& /*value*/)
{
  throw std::runtime_error(
    "storage_read_container::set_attribute does not accept any attributes for a container named " +
    name_);
}
