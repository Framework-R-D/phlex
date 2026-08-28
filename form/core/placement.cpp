// Copyright (C) 2025 ...

#include "placement.hpp"

#include <string>
#include <string_view>
#include <utility>

using namespace form::detail::experimental;

/// Constructor with initialization
placement::placement(std::string file_name, std::string container_name, technology::id technology) :
  technology_(technology),
  file_name_(std::move(file_name)),
  container_name_(std::move(container_name))
{
}

/// Access file name
std::string const& placement::file_name() const { return file_name_; }
/// Access container name
std::string const& placement::container_name() const { return container_name_; }
/// Access technology type
form::technology::id placement::technology() const { return technology_; }

std::string form::detail::experimental::build_full_label(std::string_view creator,
                                                         std::string_view label)
{
  std::string result;
  result.reserve(creator.size() + 1 + label.size());
  result += creator;
  result += '/';
  result += label;
  return result;
}
