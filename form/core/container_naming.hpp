// Copyright (C) 2025 ...

#ifndef FORM_CORE_CONTAINER_NAMING_HPP
#define FORM_CORE_CONTAINER_NAMING_HPP

#include <stdexcept>
#include <string>
#include <string_view>

namespace form::detail::experimental {

  /// Builds a container name as "row_space/label".
  inline std::string build_full_label(std::string_view row_space, std::string_view label)
  {
    std::string result;
    result.reserve(row_space.size() + 1 + label.size());
    result += row_space;
    result += '/';
    result += label;
    return result;
  }

  /// Builds the physical row-space name as "creator_stage".
  inline std::string build_row_space_name(std::string_view creator, std::string_view stage)
  {
    // '/' is reserved as the row-space/label separator.
    if (creator.contains('/') || stage.contains('/')) {
      throw std::runtime_error("FORM: creator '" + std::string{creator} + "' and stage '" +
                               std::string{stage} + "' cannot contain '/'");
    }
    std::string result;
    result.reserve(creator.size() + 1 + stage.size());
    result += creator;
    result += '_';
    result += stage;
    return result;
  }

} // namespace form::detail::experimental

#endif // FORM_CORE_CONTAINER_NAMING_HPP
