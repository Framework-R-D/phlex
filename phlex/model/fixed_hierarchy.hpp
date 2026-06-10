#ifndef PHLEX_MODEL_FIXED_HIERARCHY_HPP
#define PHLEX_MODEL_FIXED_HIERARCHY_HPP

#include "phlex/phlex_model_export.hpp"

#include "phlex/model/fwd.hpp"
#include "phlex/model/layer_path.hpp"

#include <cstddef>
#include <initializer_list>
#include <string>
#include <vector>

namespace phlex {

  /// @brief Cursor-like object for traversing the data-cell hierarchy.
  ///
  /// Represents a position in the hierarchy and provides `yield_child()` to emit
  /// child data cells and advance the cursor. Used by driver functions that require
  /// hierarchical traversal of the data-cell tree.
  ///
  /// @see data_cell_yielder for a callable alternative that accepts indices directly
  class PHLEX_MODEL_EXPORT data_cell_cursor {
  public:
    // Validates that the child layer is part of the fixed hierarchy and yields the child
    // data-cell index to the underlying driver, returning a data_cell_cursor for the child.
    data_cell_cursor yield_child(std::string const& layer_name, std::size_t number) const;

    experimental::layer_path layer_path() const;

  private:
    friend class fixed_hierarchy;
    data_cell_cursor(data_cell_index_ptr index,
                     fixed_hierarchy const& h,
                     experimental::framework_driver& d);

    data_cell_index_ptr index_;
    // Non-owning references to the enclosing hierarchy and driver; data_cell_cursor is a
    // short-lived view and does not manage their lifetimes.
    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
    fixed_hierarchy const& hierarchy_;
    experimental::framework_driver& driver_;
    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
  };

  /// @brief Callable object that yields data cells to the framework driver.
  ///
  /// Bound to a @c fixed_hierarchy and @c framework_driver, this callable accepts
  /// a @c data_cell_index_ptr and validates it against the hierarchy before yielding
  /// it to the driver. Used by driver functions that need to emit multiple data cells
  /// without manually managing the job-level cursor.
  ///
  /// @see data_cell_cursor for a cursor-based alternative
  class PHLEX_MODEL_EXPORT data_cell_yielder {
  public:
    void operator()(data_cell_index_ptr const& index) const;

  private:
    friend class fixed_hierarchy;
    data_cell_yielder(fixed_hierarchy const& h, experimental::framework_driver& d);

    // NOLINTBEGIN(cppcoreguidelines-avoid-const-or-ref-data-members)
    fixed_hierarchy const& hierarchy_;
    experimental::framework_driver& driver_;
    // NOLINTEND(cppcoreguidelines-avoid-const-or-ref-data-members)
  };

  class PHLEX_MODEL_EXPORT fixed_hierarchy {
  public:
    fixed_hierarchy() = default;
    // Using an std::initializer_list removes one set of braces that the
    // user must provide; leaving it explicit preserves clarity of
    // expression for callers.
    // NOLINTNEXTLINE(performance-google-explicit-constructor)
    explicit fixed_hierarchy(std::initializer_list<std::vector<std::string>> layer_paths);
    explicit fixed_hierarchy(std::vector<std::vector<std::string>> layer_paths);

    // Returns the layer paths for this fixed hierarchy.
    auto const& layer_paths() const { return layer_paths_; }

    void validate(data_cell_index_ptr const& index) const;

    // Yields the job-level data-cell index to the provided driver and returns a
    // data_cell_cursor for the job.
    data_cell_cursor yield_job(experimental::framework_driver& d) const;

    // Returns a callable data-cell yielder bound to the provided driver.
    data_cell_yielder yielder(experimental::framework_driver& d) const;

    // Merges additional layer paths into the hierarchy, ignoring duplicates.
    void update(std::vector<experimental::layer_path> layer_paths);

  private:
    std::vector<experimental::layer_path> layer_paths_;
    std::vector<std::size_t> layer_hashes_;
  };

}

#endif // PHLEX_MODEL_FIXED_HIERARCHY_HPP
