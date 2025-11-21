#ifndef PHLEX_MODEL_DATA_LAYER_HIERARCHY_HPP
#define PHLEX_MODEL_DATA_LAYER_HIERARCHY_HPP

#include "phlex/model/data_cell_id.hpp"
#include "phlex/model/fwd.hpp"

#include "oneapi/tbb/concurrent_unordered_map.h"

#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace phlex::experimental {

  class data_layer_hierarchy {
  public:
    ~data_layer_hierarchy();
    void increment_count(data_cell_id_ptr const& id);
    std::size_t count_for(std::string const& level_name) const;

    void print() const;

  private:
    std::string graph_layout() const;

    using hash_name_pair = std::pair<std::string, std::size_t>;
    using hash_name_pairs = std::vector<hash_name_pair>;
    std::string pretty_recurse(std::map<std::string, hash_name_pairs> const& tree,
                               std::string const& parent_name,
                               std::string indent = {}) const;

    struct layer_entry {
      layer_entry(std::string n, std::size_t par_hash) : name{std::move(n)}, parent_hash{par_hash}
      {
      }

      std::string name;
      std::size_t parent_hash;
      std::atomic<std::size_t> count{};
    };

    tbb::concurrent_unordered_map<std::size_t, std::shared_ptr<layer_entry>> levels_;
  };

}

#endif // PHLEX_MODEL_DATA_LAYER_HIERARCHY_HPP
