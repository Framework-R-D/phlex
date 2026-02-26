#ifndef PHLEX_MODEL_DATA_CELL_COUNTER_HPP
#define PHLEX_MODEL_DATA_CELL_COUNTER_HPP

#include "phlex/model/data_cell_index.hpp"
#include "phlex/model/fwd.hpp"

#include "oneapi/tbb/concurrent_hash_map.h"

#include <cstddef>
#include <map>
#include <optional>

namespace phlex::experimental {
  class flush_counts {
  public:
    flush_counts();
    explicit flush_counts(std::map<data_cell_index::hash_type, std::size_t> child_counts);

    auto begin() const { return child_counts_.begin(); }
    auto end() const { return child_counts_.end(); }
    bool empty() const { return child_counts_.empty(); }
    auto size() const { return child_counts_.size(); }

    std::optional<std::size_t> count_for(data_cell_index::hash_type const layer_hash) const
    {
      if (auto it = child_counts_.find(layer_hash); it != child_counts_.end()) {
        return it->second;
      }
      return std::nullopt;
    }

  private:
    std::map<data_cell_index::hash_type, std::size_t> child_counts_{};
  };

  class data_cell_counter {
  public:
    data_cell_counter();
    data_cell_counter(data_cell_counter* parent, std::string const& layer_name);
    ~data_cell_counter();

    data_cell_counter make_child(std::string const& layer_name);
    flush_counts result() const
    {
      if (empty(child_counts_)) {
        return flush_counts{};
      }
      return flush_counts{child_counts_};
    }

  private:
    void adjust(data_cell_counter& child);

    data_cell_counter* parent_;
    data_cell_index::hash_type layer_hash_;
    std::map<data_cell_index::hash_type, std::size_t> child_counts_{};
  };

  class flush_counters {
  public:
    void update(data_cell_index_ptr const id);
    flush_counts extract(data_cell_index_ptr const id);

  private:
    std::map<data_cell_index::hash_type, std::shared_ptr<data_cell_counter>> counters_;
  };
}

#endif // PHLEX_MODEL_DATA_CELL_COUNTER_HPP
