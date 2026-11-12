// Copyright (C) 2025 ...

#include "descriptor.hpp"

#include "core/technology.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace form::detail::experimental {

  void check_layout_version(std::string_view version, std::string const& file_name)
  {
    if (version != form_layout_version) {
      throw std::runtime_error("FORM: file '" + file_name + "' declares layout version '" +
                               std::string{version} +
                               "', which this build cannot read; it "
                               "reads layout version " +
                               std::string{form_layout_version});
    }
  }

  std::vector<form::technology::id> parse_technologies(std::string_view text,
                                                       std::string const& file_name)
  {
    if (text.empty()) {
      throw std::runtime_error("FORM: the layout descriptor in file '" + file_name +
                               "' names no storage technology");
    }

    std::vector<form::technology::id> technologies;
    std::size_t start = 0;
    while (start <= text.size()) {
      auto const comma = text.find(descriptor_technology_separator, start);
      auto const token = text.substr(
        start, comma == std::string_view::npos ? std::string_view::npos : comma - start);
      auto const technology = form::technology::find_on_disk_token(token);
      if (!technology) {
        throw std::runtime_error("FORM: the layout descriptor in file '" + file_name +
                                 "' names an unknown storage technology '" + std::string{token} +
                                 "'");
      }
      technologies.push_back(*technology);
      if (comma == std::string_view::npos) {
        break;
      }
      start = comma + 1;
    }
    return technologies;
  }

  std::optional<std::string> descriptor_entry(
    std::vector<std::pair<std::string, std::string>> const& entries, std::string_view key)
  {
    for (auto const& [name, value] : entries) {
      if (name == key) {
        return value;
      }
    }
    return std::nullopt;
  }

}
