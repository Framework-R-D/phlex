// Copyright (C) 2025 ...

#ifndef FORM_PERSISTENCE_DESCRIPTOR_HPP
#define FORM_PERSISTENCE_DESCRIPTOR_HPP

#include <string_view>

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
  inline constexpr std::string_view form_layout_version = "1.0";

  /// Keys written into the descriptor.
  inline constexpr std::string_view descriptor_layout_version_key = "form_layout_version";
  inline constexpr std::string_view descriptor_technologies_key = "technologies";

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_DESCRIPTOR_HPP
