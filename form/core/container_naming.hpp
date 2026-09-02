// Copyright (C) 2025 ...

#ifndef FORM_CORE_CONTAINER_NAMING_HPP
#define FORM_CORE_CONTAINER_NAMING_HPP

#include <string>
#include <string_view>

namespace form::detail::experimental {

  /// The container-name convention shared by writer and reader: creator and label joined as
  /// "creator/label". FORM builds a placement's container name with it on write; the reader
  /// resolves the same name back.
  inline std::string build_full_label(std::string_view creator, std::string_view label)
  {
    std::string result;
    result.reserve(creator.size() + 1 + label.size());
    result += creator;
    result += '/';
    result += label;
    return result;
  }

} // namespace form::detail::experimental

#endif // FORM_CORE_CONTAINER_NAMING_HPP
