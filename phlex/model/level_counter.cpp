#include "phlex/model/level_counter.hpp"
#include "phlex/model/fwd.hpp"
#include "phlex/model/level_id.hpp"
#include "phlex/utilities/hashing.hpp"

#include <cassert>
#include <cstddef>
#include <iterator>
#include <map>
#include <memory>
#include <utility>

namespace phlex::experimental {

  flush_counts::flush_counts() = default;

  flush_counts::flush_counts(std::map<level_id::hash_type, std::size_t> child_counts) :
    child_counts_{std::move(child_counts)}
  {
  }

  level_counter::level_counter() : level_counter{nullptr, "job"} {}

  level_counter::level_counter(level_counter* parent, std::string const& level_name) :
    parent_{parent}, level_hash_{parent_ ? hash(parent->level_hash_, level_name) : hash(level_name)}
  {
  }

  level_counter::~level_counter()
  {
    if (parent_) {
      parent_->adjust(*this);
    }
  }

  auto level_counter::make_child(std::string const& level_name) -> level_counter
  {
    return {this, level_name};
  }

  void level_counter::adjust(level_counter& child)
  {
    auto it2 = child_counts_.find(child.level_hash_);
    if (it2 == cend(child_counts_)) {
      it2 = child_counts_.try_emplace(child.level_hash_, 0).first;
    }
    ++it2->second;
    for (auto const& [nested_level_hash, count] : child.child_counts_) {
      child_counts_[nested_level_hash] += count;
    }
  }

  void flush_counters::update(level_id_ptr const id)
  {
    level_counter* parent_counter = nullptr;
    if (auto parent = id->parent()) {
      auto it = counters_.find(parent->hash());
      assert(it != counters_.cend());
      parent_counter = it->second.get();
    }
    counters_[id->hash()] = std::make_shared<level_counter>(parent_counter, id->level_name());
  }

  auto flush_counters::extract(level_id_ptr const id) -> flush_counts
  {
    auto counter = counters_.extract(id->hash());
    return counter.mapped()->result();
  }
}
