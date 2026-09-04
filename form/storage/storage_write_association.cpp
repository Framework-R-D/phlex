// Copyright (C) 2025 ...

#include "storage_write_association.hpp"

using namespace form::detail::experimental;

namespace {
  std::string maybe_remove_suffix(std::string const& name)
  {
    auto del_pos = name.find('/');
    if (del_pos != std::string::npos) {
      return name.substr(0, del_pos);
    }
    return name;
  }
}

storage_write_association::storage_write_association(std::string const& name) :
  storage_write_container::storage_write_container(maybe_remove_suffix(name))
{
}

void storage_write_association::set_attribute(std::string const& /*key*/,
                                              std::string const& /*value*/)
{
}
