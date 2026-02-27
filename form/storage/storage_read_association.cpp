// Copyright (C) 2025 ...

#include "storage_read_association.hpp"

using namespace form::detail::experimental;

namespace {
  std::string maybe_remove_suffix(std::string const& name)
  {
    auto del_pos = name.find("/");
    if (del_pos != std::string::npos) {
      return name.substr(0, del_pos);
    }
    return name;
  }
}

Storage_Read_Association::Storage_Read_Association(std::string const& name) :
  Storage_Read_Container::Storage_Read_Container(maybe_remove_suffix(name))
{
}

void Storage_Read_Association::setAttribute(std::string const& /*key*/, std::string const& /*value*/) {}
