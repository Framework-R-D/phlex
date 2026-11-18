// Copyright (C) 2025 ...

#include "descriptor.hpp"

#include "core/technology.hpp"
#include "util/error.hpp"

#include <cstddef>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace form::detail::experimental {

  void check_layout_version(std::string_view version, std::string const& file_name)
  {
    if (version != form_layout_version) {
      error("file '" + file_name + "' declares layout version '" +
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
      error("the layout descriptor in file '" + file_name +
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
        error("the layout descriptor in file '" + file_name +
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

  std::optional<std::string> descriptor_entry(descriptor_entries const& entries,
                                              std::string_view key)
  {
    for (auto const& [name, value] : entries) {
      if (name == key) {
        return value;
      }
    }
    return std::nullopt;
  }

  descriptor::descriptor(std::string file_name) : file_name_(std::move(file_name)) {}

  std::string const& descriptor::file_name() const { return file_name_; }

  void descriptor::write(descriptor_entries const& entries)
  {
    std::set<std::string_view> keys;
    for (auto const& [key, value] : entries) {
      if (!keys.insert(key).second) {
        error("descriptor key '" + key +
              "' appears more than once for " + file_name_);
      }
    }
    store(entries);
  }

  std::optional<descriptor_entries> memory_descriptor::read() { return entries_; }

  void memory_descriptor::store(descriptor_entries const& entries) { entries_ = entries; }

} // namespace form::detail::experimental
