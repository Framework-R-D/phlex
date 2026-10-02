// Copyright (C) 2025 ...

#include "storage_write_association.hpp"

#include "core/container_naming.hpp"
#include "storage/storage_write_container.hpp"

#include <string>

using namespace form::detail::experimental;

storage_write_association::storage_write_association(std::string const& name) :
  storage_write_container::storage_write_container(row_space_of(name))
{
}

void storage_write_association::set_attribute(std::string const& /*key*/,
                                              std::string const& /*value*/)
{
}
