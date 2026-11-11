// Copyright (C) 2025 ...

#ifndef FORM_PERSISTENCE_DESCRIPTOR_HPP
#define FORM_PERSISTENCE_DESCRIPTOR_HPP

#include "core/technology.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/* @file descriptor.hpp
 * @brief Defines the fixed keys and names for the FORM layout descriptor.
 *
 * The descriptor is stored in a backend-native form and identifies the layout used to interpret the
 * rest of the file.
 */
namespace form::detail::experimental {

  inline constexpr std::string_view form_block_prefix = "form_";

  /// Backend-native object name reserved for the FORM layout descriptor.
  inline constexpr std::string_view descriptor_object_name = "form_descriptor";

  /// On-disk layout version, independent of the Phlex release version.
  inline constexpr std::string_view form_layout_version = "0.1";

  /// Keys written into the descriptor.
  inline constexpr std::string_view descriptor_layout_version_key = "form_layout_version";
  inline constexpr std::string_view descriptor_technologies_key = "technologies";

  inline constexpr char descriptor_technology_separator = ',';

  /// Throws if the file declares an unsupported layout version.
  void check_layout_version(std::string_view version, std::string const& file_name);

  std::vector<form::technology::id> parse_technologies(std::string_view text,
                                                       std::string const& file_name);

  std::optional<std::string> descriptor_entry(
    std::vector<std::pair<std::string, std::string>> const& entries, std::string_view key);

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_DESCRIPTOR_HPP
