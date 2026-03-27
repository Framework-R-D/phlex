#ifndef PHLEX_MODEL_FIXED_HIERARCHY_HPP
#define PHLEX_MODEL_FIXED_HIERARCHY_HPP

#include "phlex/phlex_model_export.hpp"

#include "phlex/model/fwd.hpp"

#include <cstddef>
#include <initializer_list>
#include <string>
#include <vector>

namespace phlex::experimental {
  template <typename RT>
  class async_driver;
}

namespace phlex {

  class fixed_hierarchy;

  class phlex_model_EXPORT data_cell_cursor {
  public:
    // Validates that the child layer is part of the fixed hierarchy and yields the child
    // data-cell index to the underlying driver, returning a data_cell_cursor for the child.
    data_cell_cursor yield_child(std::string const& layer_name, std::size_t number) const;

    std::string layer_path() const;

  private:
    friend class fixed_hierarchy;
    data_cell_cursor(data_cell_index_ptr index,
                     fixed_hierarchy const& h,
                     experimental::async_driver<data_cell_index_ptr>& d);

    data_cell_index_ptr index_;
    fixed_hierarchy const& hierarchy_;
    experimental::async_driver<data_cell_index_ptr>& driver_;
  };

  class phlex_model_EXPORT fixed_hierarchy {
  public:
    fixed_hierarchy() = default;
    // Using an std::initializer_list removes one set of braces that the user must provide
    explicit fixed_hierarchy(std::initializer_list<std::vector<std::string>> layer_paths);
    explicit fixed_hierarchy(std::vector<std::vector<std::string>> layer_paths);

    void validate(data_cell_index_ptr const& index) const;

    // Yields the job-level data-cell index to the provided driver and returns a
    // data_cell_cursor for the job.  Must only be called from a function registered
    // via driver_proxy::drive().
    data_cell_cursor yield_job(experimental::async_driver<data_cell_index_ptr>& d) const;

  private:
    std::vector<std::size_t> layer_hashes_;
  };

}

#endif // PHLEX_MODEL_FIXED_HIERARCHY_HPP
