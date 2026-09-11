// Copyright (C) 2025 ...

#ifndef FORM_CORE_CELL_INDEX_HPP
#define FORM_CORE_CELL_INDEX_HPP

#include <cctype>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/* @file cell_index.hpp
 * @brief Representation of a data cell used by FORM's lower layers.
 *
 * The canonical cell ID is preserved as rendered by the framework.
 * Layer names come from the cell itself; FORM does not assume a particular layer tuple.
 */
namespace form::detail::experimental {

  struct cell_index {
    /// Canonical text representation of the cell
    std::string id;
    /// Layer names, from outermost to innermost.
    std::vector<std::string> layer_names;
    /// Layer values corresponding to layer_names.
    std::vector<std::uint64_t> layer_values;

    bool is_job() const { return layer_names.empty(); }
    /// Whether layer names and values have matching sizes.
    bool consistent() const { return layer_names.size() == layer_values.size(); }
  };

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

  /// Return the hierarchy key formed from the ordered, sanitized layer names.
  /// Cells with the same hierarchy key share a navigation container.
  inline std::string hierarchy_key(std::vector<std::string> const& layer_names)
  {
    // Give the job cell a dedicated key.
    if (layer_names.empty()) {
      return "job";
    }

    std::string key;
    for (auto const& layer_name : layer_names) {
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

  /// Container-name prefix reserved for FORM's navigation containers.
  inline constexpr std::string_view navigation_prefix = "nav_";

  /// Return the navigation-table name for a hierarchy.
  inline std::string navigation_table_name(std::string_view hierarchy)
  {
    std::string name{navigation_prefix};
    name += "cells_";
    name += hierarchy;
    return name;
  }

  /// Name of the product dictionary.
  inline constexpr std::string_view navigation_dictionary_name = "nav_products";

  /// Return a name for an unnamed layer.
  inline std::string unnamed_layer_name(std::size_t position)
  {
    return "layer" + std::to_string(position);
  }

} // namespace form::detail::experimental

#endif // FORM_CORE_CELL_INDEX_HPP
