#include "phlex/model/data_cell_counter.hpp"
#include "phlex/utilities/hashing.hpp"

#include <cassert>

namespace phlex::experimental {

  flush_counts::flush_counts() = default;

  flush_counts::flush_counts(std::map<data_cell_id::hash_type, std::size_t> child_counts) :
    child_counts_{std::move(child_counts)}
  {
  }

  data_cell_counter::data_cell_counter() : data_cell_counter{nullptr, "job"} {}

  data_cell_counter::data_cell_counter(data_cell_counter* parent, std::string const& layer_name) :
    parent_{parent}, layer_hash_{parent_ ? hash(parent->layer_hash_, layer_name) : hash(layer_name)}
  {
  }

  data_cell_counter::~data_cell_counter()
  {
    if (parent_) {
      parent_->adjust(*this);
    }
  }

  data_cell_counter data_cell_counter::make_child(std::string const& layer_name)
  {
    return {this, layer_name};
  }

  void data_cell_counter::adjust(data_cell_counter& child)
  {
    auto it2 = child_counts_.find(child.layer_hash_);
    if (it2 == cend(child_counts_)) {
      it2 = child_counts_.try_emplace(child.layer_hash_, 0).first;
    }
    ++it2->second;
    for (auto const& [nested_layer_hash, count] : child.child_counts_) {
      child_counts_[nested_layer_hash] += count;
    }
  }

  void flush_counters::update(data_cell_id_ptr const id)
  {
    data_cell_counter* parent_counter = nullptr;
    if (auto parent = id->parent()) {
      auto it = counters_.find(parent->hash());
      assert(it != counters_.cend());
      parent_counter = it->second.get();
    }
    counters_[id->hash()] = std::make_shared<data_cell_counter>(parent_counter, id->layer_name());
  }

  flush_counts flush_counters::extract(data_cell_id_ptr const id)
  {
    auto counter = counters_.extract(id->hash());
    return counter.mapped()->result();
  }
}
