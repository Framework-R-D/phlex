#include "phlex/model/fixed_hierarchy.hpp"

#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/utilities/hashing.hpp"

#include "fmt/format.h"

#include <algorithm>
#include <ranges>
#include <set>
#include <stdexcept>

namespace {
  std::set<std::size_t> build_hashes(std::vector<std::vector<std::string>> const& layer_paths)
  {
    using namespace phlex::experimental;
    identifier const job{"job"};
    std::set<std::size_t> hashes{job.hash()};
    for (std::vector<std::string> const& path : layer_paths) {
      bool const has_job_prefix = !path.empty() && path[0] == "job";
      std::size_t cumulative_hash = job.hash();
      for (auto const& name : path | std::views::drop(has_job_prefix ? 1 : 0)) {
        cumulative_hash = hash(cumulative_hash, identifier{name}.hash());
        hashes.insert(cumulative_hash);
      }
    }
    return hashes;
  }
}

namespace phlex::experimental {

  fixed_hierarchy::fixed_hierarchy(std::initializer_list<std::vector<std::string>> layer_paths) :
    fixed_hierarchy(std::vector<std::vector<std::string>>(layer_paths))
  {
  }

  fixed_hierarchy::fixed_hierarchy(std::vector<std::vector<std::string>> layer_paths) :
    layer_hashes_(std::from_range, build_hashes(layer_paths))
  {
  }

  void fixed_hierarchy::validate(data_cell_index_ptr const& index) const
  {
    if (layer_hashes_.empty()) {
      return;
    }
    if (std::ranges::binary_search(layer_hashes_, index->layer_hash())) {
      return;
    }
    throw std::runtime_error(
      fmt::format("Layer {} is not part of the fixed hierarchy.", index->layer_path()));
  }

}
