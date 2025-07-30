// Copyright (C) 2025 ...

#include "storage_association.hpp"

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

Storage_Association::Storage_Association(const std::string& name) :
  Storage_Container::Storage_Container(maybe_remove_suffix(name))
{
}
