#ifndef PHLEX_MODEL_FIXED_HIERARCHY_HPP
#define PHLEX_MODEL_FIXED_HIERARCHY_HPP

#include "phlex/model/fwd.hpp"

#include <cstddef>
#include <initializer_list>
#include <string>
#include <vector>

namespace phlex::experimental {

  class fixed_hierarchy {
  public:
    fixed_hierarchy() = default;
    // Using an std::initializer_list removes one set of braces that the user must provide
    explicit fixed_hierarchy(std::initializer_list<std::vector<std::string>> layer_paths);
    explicit fixed_hierarchy(std::vector<std::vector<std::string>> layer_paths);

    auto validator() const
    {
      return [this](data_cell_index_ptr const& index) { validate(index); };
    }

    void validate(data_cell_index_ptr const& index) const;

  private:
    std::vector<std::size_t> layer_hashes_;
  };

}

#endif // PHLEX_MODEL_FIXED_HIERARCHY_HPP
