#include "phlex/model/fixed_hierarchy.hpp"

#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/utilities/hashing.hpp"
#include "phlex/utilities/resumable_driver.hpp"

#include "fmt/format.h"

#include <algorithm>
#include <ranges>
#include <set>
#include <span>
#include <stdexcept>

namespace {
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
  std::set<std::size_t> build_hashes(
    std::vector<phlex::experimental::layer_path> const& layer_paths)
  {
    using namespace phlex::experimental;
    using namespace phlex::experimental::literals;
    std::set<std::size_t> hashes{"job"_idq.hash};
    for (layer_path const& path : layer_paths) {
      hashes.merge(path.hashes());
    }
    return hashes;
  }

  std::vector<phlex::experimental::layer_path> convert_vector_vector_string(
    std::vector<std::vector<std::string>>&& layer_paths)
  {
    using namespace phlex::experimental;
    return std::move(layer_paths) | std::views::transform([](std::vector<std::string>& lp) {
             auto lp_as_ids =
               lp | std::views::transform([](auto& str) { return identifier(std::move(str)); }) |
               std::ranges::to<std::vector>();
             return layer_path(std::move(lp_as_ids));
           }) |
           std::ranges::to<std::vector<layer_path>>();
  }
}

namespace phlex {
  // ================================================================================
  // data_cell_cursor implementation
  data_cell_cursor::data_cell_cursor(data_cell_index_ptr index,
                                     fixed_hierarchy const& h,
                                     experimental::framework_driver& d) :
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

  experimental::layer_path data_cell_cursor::layer_path() const { return index_->layer_path(); }

  // ================================================================================
  // data_cell_yielder implementation
  data_cell_yielder::data_cell_yielder(fixed_hierarchy const& h,
                                       experimental::framework_driver& d) :
    hierarchy_{h}, driver_{d}
  {
  }

  void data_cell_yielder::operator()(data_cell_index_ptr const& index) const
  {
    hierarchy_.validate(index);
    driver_.yield(index);
  }

  // ================================================================================
  // fixed_hierarchy implementation
  fixed_hierarchy::fixed_hierarchy(std::initializer_list<std::vector<std::string>> layer_paths) :
    fixed_hierarchy(std::vector<std::vector<std::string>>(layer_paths))
  {
  }

  fixed_hierarchy::fixed_hierarchy(std::vector<std::vector<std::string>> layer_paths) :
    layer_paths_(convert_vector_vector_string(std::move(layer_paths))),
    layer_hashes_(std::from_range, build_hashes(layer_paths_))
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

  data_cell_cursor fixed_hierarchy::yield_job(experimental::framework_driver& d) const
  {
    auto job = data_cell_index::job();
    d.yield(job);
    return data_cell_cursor{job, *this, d};
  }

  data_cell_yielder fixed_hierarchy::yielder(experimental::framework_driver& d) const
  {
    return data_cell_yielder{*this, d};
  }
}
