// Copyright (C) 2025 ...

#include "storage_read_container.hpp"
#include "storage_file.hpp"

using namespace form::detail::experimental;

storage_read_container::storage_read_container(std::string const& name) :
  name_(name), file_(nullptr)
{
  auto del_pos = name.find('/');
  if (del_pos != std::string::npos) {
    t_name_ = name.substr(0, del_pos);
    c_name_ = name.substr(del_pos + 1);
  } else {
    t_name_ = name;
    c_name_ = "Main";
  }
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
