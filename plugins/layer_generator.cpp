#include "plugins/layer_generator.hpp"
#include "phlex/model/data_cell_index.hpp"

#include "fmt/format.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <ranges>

namespace phlex::experimental {

  layer_generator::layer_generator()
  {
    // Always seed the "job" in case only the job is desired
    parent_to_children_["/job"] = {};
    emitted_cells_["/job"] = 0ull;
  }

  std::size_t layer_generator::emitted_cell_count(std::string layer_path) const
  {
    // Check if the count of all emitted cells is requested
    if (layer_path.empty()) {
      // For C++23, we can use std::ranges::fold_left
      std::size_t total{};
      for (auto const& [_, count] : emitted_cells_) {
        total += count;
      }
      return total;
    }

    if (auto it = emitted_cells_.find(layer_path); it != emitted_cells_.end()) {
      return emitted_cells_.at(layer_path);
    }

    throw std::runtime_error("No emitted cells corresponding to layer path '" + layer_path + "'");
  }

  std::string layer_generator::parent_path(std::string const& layer_name,
                                           std::string const& parent_layer_spec) const
  {
    // Seed result with the specified parent_layer_spec
    std::string result{"/" + parent_layer_spec};
    std::string const* found_parent{nullptr};
    for (auto const& path : layer_paths_) {
      if (path.ends_with(parent_layer_spec)) {
        if (found_parent) {
          auto const msg =
            fmt::format("Ambiguous: two parent layers found for data layer '{}':\n  - {}\n  - {}"
                        "\nTo disambiguate, specify a parent layer path that is more complete.",
                        layer_name,
                        *found_parent,
                        path);
          throw std::runtime_error(msg);
        }
        found_parent = &path;
      }
    }
    return found_parent ? *found_parent : result;
  }

  void layer_generator::maybe_rebase_layer_paths(std::string const& layer_name,
                                                 std::string const& parent_full_path)
  {
    // First check if layer paths need to be rebased
    std::vector<size_t> indices_for_rebasing;
    for (std::size_t i = 0ull, n = layer_paths_.size(); i != n; ++i) {
      auto const& layer = layer_paths_[i];
      if (layer.starts_with("/" + layer_name)) {
        indices_for_rebasing.push_back(i);
      }
    }

    // Do the rebase
    for (std::size_t const i : indices_for_rebasing) {
      auto const old_layer_path = layer_paths_[i];
      auto const& new_layer_path = layer_paths_[i] = parent_full_path + old_layer_path;

      auto layer_handle = layers_.extract(old_layer_path);
      layer_handle.key() = new_layer_path;
      auto const old_parent_path = layer_handle.mapped().parent_layer_name;
      auto const new_parent_path = new_layer_path.substr(0, new_layer_path.find_last_of("/"));
      layer_handle.mapped().parent_layer_name = new_parent_path;
      layers_.insert(std::move(layer_handle));

      auto emitted_handle = emitted_cells_.extract(old_layer_path);
      emitted_handle.key() = new_layer_path;
      emitted_cells_.insert(std::move(emitted_handle));

      auto reverse_handle = parent_to_children_.extract(old_parent_path);
      reverse_handle.key() = new_parent_path;
      parent_to_children_.insert(std::move(reverse_handle));
    }
  }

  void layer_generator::add_layer(std::string layer_name, layer_spec lspec)
  {
    auto const parent_full_path = parent_path(layer_name, lspec.parent_layer_name);

    // We need to make sure that we can distinguish between (e.g.) /events and /run/events.
    // When a layer is added, the parent layers are also included as part of the path.
    maybe_rebase_layer_paths(layer_name, parent_full_path);

    auto full_path = parent_full_path + "/" + layer_name;

    lspec.parent_layer_name = parent_full_path;
    layers_[full_path] = std::move(lspec);
    emitted_cells_[full_path] = 0ull;
    parent_to_children_[parent_full_path].push_back(std::move(layer_name));
    layer_paths_.push_back(full_path);
  }

  void layer_generator::execute(framework_driver& driver, data_cell_index_ptr index, bool recurse)
  {
    auto cell_it = emitted_cells_.find(index->layer_path());
    assert(cell_it != emitted_cells_.cend());
    ++cell_it->second;

    driver.yield(index);

    if (not recurse) {
      return;
    }

    auto it = parent_to_children_.find(index->layer_path());
    assert(it != parent_to_children_.cend());

    for (auto const& child : it->second) {
      auto const full_child_path = index->layer_path() + "/" + child;
      bool const recurse = parent_to_children_.contains(full_child_path);
      auto const& [_, total_per_parent, starting_value] = layers_.at(full_child_path);
      for (unsigned int i : std::views::iota(starting_value, total_per_parent + starting_value)) {
        execute(driver, index->make_child(i, child), recurse);
      }
    }
  }

}
