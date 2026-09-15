// Copyright (C) 2025 ...

#ifndef FORM_PERSISTENCE_NAVIGATION_NAMING_HPP
#define FORM_PERSISTENCE_NAVIGATION_NAMING_HPP

#include "core/cell_index.hpp"
#include "core/technology.hpp"

#include <cctype>
#include <string>
#include <string_view>

/* @file navigation_naming.hpp
 * @brief Naming helpers for FORM's navigation containers.
 */
namespace form::detail::experimental {

  /// Prefix for FORM navigation container names.
  inline constexpr std::string_view navigation_prefix = "nav_";

  /// Return the lowercase technology token for container names.
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

  /// Flatten a hierarchy for use in a container name.
  inline std::string hierarchy_key(cell_hierarchy const& hierarchy)
  {
    if (hierarchy.layer_names.empty()) {
      return "job";
    }

    std::string key;
    for (auto const& layer_name : hierarchy.layer_names) {
      if (!key.empty()) {
        key += '_';
      }
      key += sanitize_name(layer_name);
    }
    return key;
  }

  /// Return the navigation-table column name for a creator.
  inline std::string navigation_row_column(std::string_view creator)
  {
    return sanitize_name(creator) + "_row";
  }

  /// Return the navigation-table name for a hierarchy and technology.
  inline std::string navigation_table_name(cell_hierarchy const& hierarchy,
                                           form::technology::id tech)
  {
    std::string name{navigation_prefix};
    name += technology_name(tech);
    name += "_cells_";
    name += hierarchy_key(hierarchy);
    return name;
  }

  /// Return the product-dictionary name for a technology.
  inline std::string navigation_dictionary_name(form::technology::id tech)
  {
    std::string name{navigation_prefix};
    name += technology_name(tech);
    name += "_products";
    return name;
  }

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_NAVIGATION_NAMING_HPP
