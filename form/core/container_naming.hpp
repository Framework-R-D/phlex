// Copyright (C) 2025 ...

#ifndef FORM_CORE_CONTAINER_NAMING_HPP
#define FORM_CORE_CONTAINER_NAMING_HPP

#include "core/technology.hpp"

#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace form::detail::experimental {

  /// Separates a row space from a product label in a container name: "row_space/label".
  /// Persistent: stored in the product dictionary; changing it requires a layout version.
  inline constexpr char row_space_label_separator = '/';

  /// Separates fields in a row-space name, e.g. "technology__creator__stage".
  /// Persistent: part of on-disk TTree/RNTuple names; changing it requires a layout version.
  inline constexpr std::string_view row_space_field_separator = "__";

  /// Builds a container name as "row_space/label".
  inline std::string build_full_label(std::string_view row_space, std::string_view label)
  {
    std::string result;
    result.reserve(row_space.size() + 1 + label.size());
    result += row_space;
    result += row_space_label_separator;
    result += label;
    return result;
  }

  /// Replace characters not allowed in names with '_'.
  inline std::string sanitize_name(std::string_view name)
  {
    std::string result;
    result.reserve(name.size());
    for (char c : name) {
      auto const uc = static_cast<unsigned char>(c);
      result.push_back(std::isalnum(uc) != 0 || c == '_' ? c : '_');
    }
    return result;
  }

  /// Returns the technology token used in physical names.
  inline std::string technology_name(form::technology::id tech)
  {
    if (tech.major == form::technology::major::generic) {
      return "generic";
    }
    auto name = form::technology::to_string(tech);
    for (char& c : name) {
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return sanitize_name(name);
  }

  /// Throws if a creator or stage cannot be part of a physical row-space name.
  inline void check_row_space_parts(std::string_view creator, std::string_view stage)
  {
    if (creator.contains(row_space_label_separator) || stage.contains(row_space_label_separator)) {
      throw std::runtime_error("FORM: creator '" + std::string{creator} + "' and stage '" +
                               std::string{stage} + "' cannot contain '" +
                               row_space_label_separator + "'");
    }
  }

  /// Builds the name of the (creator, stage) stream as "creator__stage".
  inline std::string build_stream_name(std::string_view creator, std::string_view stage)
  {
    check_row_space_parts(creator, stage);
    std::string result{creator};
    result += row_space_field_separator;
    result += stage;
    return result;
  }

  /// Builds a physical row-space name as "technology__creator__stage".
  inline std::string build_row_space_name(form::technology::id tech,
                                          std::string_view creator,
                                          std::string_view stage)
  {
    std::string result = technology_name(tech);
    result += row_space_field_separator;
    result += build_stream_name(creator, stage);
    return result;
  }

} // namespace form::detail::experimental

#endif // FORM_CORE_CONTAINER_NAMING_HPP
