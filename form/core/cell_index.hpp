// Copyright (C) 2025 ...

#ifndef FORM_CORE_CELL_INDEX_HPP
#define FORM_CORE_CELL_INDEX_HPP

#include <cctype>
#include <compare>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/* @file cell_index.hpp
 * @brief Representation of a data cell used by FORM's lower layers.
 *
 * Layer names come from the cell itself; FORM does not assume a particular layer tuple.
 */
namespace form::detail::experimental {

  /// Ordered layer names identifying a hierarchy.
  struct cell_hierarchy {
    /// Layer names, from outermost to innermost; empty for the job cell.
    std::vector<std::string> layer_names;

    auto operator<=>(cell_hierarchy const&) const = default;
  };

  /// Identifies a data cell as a position within a hierarchy.
  struct cell_index {
    /// Canonical cell ID as rendered by the framework.
    std::string id;
    /// The hierarchy this cell belongs to.
    cell_hierarchy hierarchy;
    /// Layer values corresponding to hierarchy.layer_names.
    std::vector<std::uint64_t> layer_values;

    bool is_job() const { return hierarchy.layer_names.empty(); }
    /// Whether layer names and values have matching sizes.
    bool consistent() const { return hierarchy.layer_names.size() == layer_values.size(); }
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

} // namespace form::detail::experimental

#endif // FORM_CORE_CELL_INDEX_HPP
