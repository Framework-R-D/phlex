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

using phlex::experimental::layer_path;

namespace {
  // Removes duplicate paths from the provided list.
  std::vector<layer_path> unique_paths(std::vector<layer_path> paths)
  {
    std::set<layer_path> const unique{std::from_range, std::move(paths)};
    return {std::from_range, std::move(unique)};
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
  std::set<std::size_t> build_hashes(std::vector<layer_path> const& layer_paths)
  {
    using namespace phlex::experimental::literals;
    std::set<std::size_t> hashes{"job"_idq.hash};
    for (layer_path const& path : layer_paths) {
      // "job" was added explicitly so it doesn't matter whether it does or does not appear in path.hashes()
      hashes.merge(path.hashes());
    }
    return hashes;
  }

  std::vector<layer_path> convert_vector_vector_string(
    std::vector<std::vector<std::string>>&& layer_path_strings)
  {
    std::vector<layer_path> layer_paths;
    layer_paths.reserve(layer_path_strings.size());
    for (auto& lp : layer_path_strings) {
      auto lp_as_ids = lp | std::views::as_rvalue | std::views::transform([](std::string&& str) {
                         return phlex::experimental::identifier(std::move(str));
                       }) |
                       std::ranges::to<std::vector>();
      layer_paths.emplace_back(std::move(lp_as_ids));
    }
    return unique_paths(std::move(layer_paths));
  }
}

namespace phlex {
  // ================================================================================
  // data_cell_cursor implementation
  data_cell_cursor::data_cell_cursor(data_cell_index_ptr index,
                                     fixed_hierarchy const& h,
                                     detail::framework_driver& d) :
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
  data_cell_yielder::data_cell_yielder(fixed_hierarchy const& h, detail::framework_driver& d) :
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

  void fixed_hierarchy::update(std::vector<layer_path> layer_paths)
  {
    std::set<layer_path> merged{std::from_range, std::move(layer_paths_)};
    merged.insert_range(std::move(layer_paths));

    layer_paths_.assign_range(merged);
    layer_hashes_.assign_range(build_hashes(layer_paths_));
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

  data_cell_cursor fixed_hierarchy::yield_job(detail::framework_driver& d) const
  {
    auto job = data_cell_index::job();
    d.yield(job);
    return data_cell_cursor{job, *this, d};
  }

  data_cell_yielder fixed_hierarchy::yielder(detail::framework_driver& d) const
  {
    return data_cell_yielder{*this, d};
  }
}
