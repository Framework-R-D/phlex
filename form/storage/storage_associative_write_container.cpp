// Copyright (C) 2025 ...

#include "storage_associative_write_container.hpp"

#include <utility>

using namespace form::detail::experimental;

storage_associative_write_container::storage_associative_write_container(std::string const& name) :
  storage_write_container::storage_write_container(name), parent_(nullptr)
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

storage_associative_write_container::~storage_associative_write_container() = default;

std::string const& storage_associative_write_container::top_name() { return t_name_; }

std::string const& storage_associative_write_container::col_name() { return c_name_; }

void storage_associative_write_container::set_parent(
  std::shared_ptr<i_storage_write_container> parent)
{
  parent_ = std::move(parent);
}
