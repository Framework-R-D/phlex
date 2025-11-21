#ifndef PHLEX_MODEL_LEVEL_COUNTER_HPP
#define PHLEX_MODEL_LEVEL_COUNTER_HPP

#include "phlex/model/data_cell_id.hpp"
#include "phlex/model/fwd.hpp"

#include "oneapi/tbb/concurrent_hash_map.h"

#include <cstddef>
#include <map>
#include <optional>

namespace phlex::experimental {
  class flush_counts {
  public:
    flush_counts();
    explicit flush_counts(std::map<data_cell_id::hash_type, std::size_t> child_counts);

    auto begin() const { return child_counts_.begin(); }
    auto end() const { return child_counts_.end(); }
    bool empty() const { return child_counts_.empty(); }
    auto size() const { return child_counts_.size(); }

    std::optional<std::size_t> count_for(data_cell_id::hash_type const level_hash) const
    {
      if (auto it = child_counts_.find(level_hash); it != child_counts_.end()) {
        return it->second;
      }
      return std::nullopt;
    }

  private:
    std::map<data_cell_id::hash_type, std::size_t> child_counts_{};
  };

  using flush_counts_ptr = std::shared_ptr<flush_counts const>;

  class level_counter {
  public:
    level_counter();
    level_counter(level_counter* parent, std::string const& level_name);
    ~level_counter();

    level_counter make_child(std::string const& level_name);
    flush_counts result() const
    {
      if (empty(child_counts_)) {
        return flush_counts{};
      }
      return flush_counts{child_counts_};
    }

  private:
    void adjust(level_counter& child);

    level_counter* parent_;
    data_cell_id::hash_type level_hash_;
    std::map<data_cell_id::hash_type, std::size_t> child_counts_{};
  };

  class flush_counters {
  public:
    void update(data_cell_id_ptr const id);
    flush_counts extract(data_cell_id_ptr const id);

  private:
    std::map<data_cell_id::hash_type, std::shared_ptr<level_counter>> counters_;
  };
}

#endif // PHLEX_MODEL_LEVEL_COUNTER_HPP
