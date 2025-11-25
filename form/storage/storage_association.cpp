// Copyright (C) 2025 ...

#include "storage_association.hpp"
#include "storage/storage_container.hpp"
#include <string>

using namespace form::detail::experimental;

namespace {
  auto maybe_remove_suffix(std::string const& name) -> std::string
  {
    auto del_pos = name.find("/");
    if (del_pos != std::string::npos) {
      return name.substr(0, del_pos);
    }
    return name;
  }
}

Storage_Association::Storage_Association(std::string const& name) :
  Storage_Container::Storage_Container(maybe_remove_suffix(name))
{
}

void Storage_Association::setAttribute(std::string const& /*key*/, std::string const& /*value*/) {}
