#include "phlex/model/fixed_hierarchy.hpp"

#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/utilities/async_driver.hpp"
#include "phlex/utilities/hashing.hpp"

#include "fmt/format.h"

#include <algorithm>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>

namespace {
  // Each path must be non-empty and may only contain "job" as the first element.
  std::span<std::string const> validated_path(std::vector<std::string> const& path)
  {
    if (path.empty()) {
      throw std::runtime_error("Layer paths cannot be empty.");
    }
    auto const rest = std::span{path}.subspan(path[0] == "job" ? 1 : 0);
    if (std::ranges::contains(rest, "job")) {
      throw std::runtime_error("Layer paths may only contain 'job' as the first element.");
    }
    return rest;
  }

  // Builds the set of cumulative layer hashes that define the fixed hierarchy.
  // For example, if the layer paths are ["job", "run", "subrun"] and ["job", "spill"],
  // the hashes included will correspond to:
  //   From the first path:
  //   - "job"
  //   - "job/run"
  //   - "job/run/subrun"
  //
  //   From the second path:
  //   - "job"        (already included from the first path)
  //   - "job/spill"
  //
  // Each path must be non-empty and may only contain "job" as the first element.
  std::set<std::size_t> build_hashes(std::vector<std::vector<std::string>> const& layer_paths)
  {
    using namespace phlex::experimental;
    identifier const job{"job"};
    std::set<std::size_t> hashes{job.hash()};
    for (std::vector<std::string> const& path : layer_paths) {
      std::size_t cumulative_hash = job.hash();
      for (auto const& name : validated_path(path)) {
        cumulative_hash = hash(cumulative_hash, identifier{name}.hash());
        hashes.insert(cumulative_hash);
      }
    }
    return hashes;
  }
}

namespace phlex::experimental {

  data_cell_cursor::data_cell_cursor(data_cell_index_ptr index,
                                     fixed_hierarchy const& h,
                                     async_driver<data_cell_index_ptr>& d) :
    index_{std::move(index)}, hierarchy_{h}, driver_{d}
  {
  }

  data_cell_cursor data_cell_cursor::yield_child(std::string const& layer_name,
                                                 std::size_t number) const
  {
    auto child = index_->make_child(layer_name, number);
    hierarchy_.validate(child);
    driver_.yield(child);
    return data_cell_cursor{child, hierarchy_, driver_};
  }

  std::string data_cell_cursor::layer_path() const { return index_->layer_path(); }

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

  data_cell_cursor fixed_hierarchy::yield_job(async_driver<data_cell_index_ptr>& d) const
  {
    auto job = data_cell_index::job();
    d.yield(job);
    return data_cell_cursor{job, *this, d};
  }

}
