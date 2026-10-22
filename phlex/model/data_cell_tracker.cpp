#include "phlex/model/data_cell_tracker.hpp"

#include "phlex/model/data_cell_counts.hpp"
#include "phlex/model/flush_messages.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/layer_path.hpp"
#include "phlex/utilities/hashing.hpp"

#include <fmt/format.h>
#include <gsl/assert>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <initializer_list>
#include <iterator>
#include <map>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <utility>
#include <vector>

using phlex::experimental::layer_path;
using layer_hash_type = phlex::data_cell_index::hash_type;

namespace {
  auto make_driver_child_layer_hashes(std::vector<layer_path> const& layer_paths)
  {
    std::map<layer_hash_type, std::vector<layer_hash_type>> result;
    for (auto const& path : layer_paths) {
      auto parent_hash = phlex::experimental::identifier{"job"}.hash();
      auto components = path.components();
      if (path.is_complete()) {
        components = components.subspan(1);
      }
      for (auto const& component : components) {
        auto const child_hash = phlex::detail::hash(parent_hash, component.hash());
        result[parent_hash].push_back(child_hash);
        parent_hash = child_hash;
      }
    }
    for (auto& child_hashes : result | std::views::values) {
      std::ranges::sort(child_hashes);
      child_hashes.erase(std::ranges::unique(child_hashes).begin(), child_hashes.end());
    }
    return result;
  }
}

namespace phlex::detail {
  data_cell_tracker::data_cell_tracker(std::initializer_list<layer_path> const layer_paths) :
    data_cell_tracker{std::vector<layer_path>(layer_paths)}
  {
  }

  data_cell_tracker::data_cell_tracker(std::vector<layer_path> const& layer_paths) :
    driver_child_layer_hashes_{make_driver_child_layer_hashes(layer_paths)}
  {
  }

  data_cell_tracker::~data_cell_tracker()
  {
    if (active_families_.empty()) {
      return;
    }
    spdlog::warn("Active families at destruction:");
    for (auto const& family : active_families_) {
      spdlog::warn("  Index: {}", family.index->to_string());
      for (auto const& [layer_hash, count] : *family.counts) {
        spdlog::warn("    {} = {}", layer_hash, count.load());
      }
    }
  }

  index_flushes data_cell_tracker::report_and_evict_ready_flushes(
    data_cell_index_ptr const& received_index)
  {
    if (received_index == nullptr) {
      index_flushes result;
      result.reserve(active_families_.size());
      while (not active_families_.empty()) {
        result.push_back(close_active_family());
      }
      return result;
    }

    // The job index starts the stream and is the only valid index without a parent.
    if (not received_index->has_parent()) {
      Expects(received_index->layer_path() == layer_path{"/job"});
      if (not active_families_.empty()) {
        throw std::runtime_error(fmt::format(
          "Received root index {} while driver families are active", received_index->to_string()));
      }
      if (driver_child_layer_hashes_.contains(received_index->layer_hash())) {
        active_families_.push_back(
          {.index = received_index, .counts = std::make_shared<data_cell_counts>()});
      }
      return {};
    }

    auto const parent = received_index->parent();
    auto const parent_family =
      std::ranges::find_if(active_families_, [&parent](index_flush const& family) {
        return family.index->hash() == parent->hash();
      });
    if (parent_family == active_families_.end()) {
      throw std::runtime_error(
        fmt::format("Received index {}, whose parent {} does not identify an active driver family",
                    received_index->to_string(),
                    parent->to_string()));
    }

    index_flushes result;
    while (std::next(parent_family) != active_families_.end()) {
      result.push_back(close_active_family());
    }

    parent_family->counts->increment(received_index->layer_hash());
    if (driver_child_layer_hashes_.contains(received_index->layer_hash())) {
      active_families_.push_back(
        {.index = received_index, .counts = std::make_shared<data_cell_counts>()});
    }
    return result;
  }

  index_flush data_cell_tracker::close_active_family()
  {
    auto family = std::move(active_families_.back());
    active_families_.pop_back();
    if (family.counts->size() == 0) {
      for (auto const child_layer_hash :
           driver_child_layer_hashes_.at(family.index->layer_hash())) {
        family.counts->emplace(child_layer_hash, 0);
      }
    }
    return family;
  }
}
