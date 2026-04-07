// Copyright (C) 2025 ...

#include "storage_write_association.hpp"

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

Storage_Write_Association::Storage_Write_Association(std::string const& name) :
  Storage_Write_Container::Storage_Write_Container(maybe_remove_suffix(name))
{
}

void Storage_Write_Association::setAttribute(std::string const& /*key*/,
                                             std::string const& /*value*/)
{
}
