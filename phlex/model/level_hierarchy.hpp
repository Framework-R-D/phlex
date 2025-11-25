#ifndef PHLEX_MODEL_LEVEL_HIERARCHY_HPP
#define PHLEX_MODEL_LEVEL_HIERARCHY_HPP

#include "phlex/model/fwd.hpp"
#include "phlex/model/level_id.hpp"

#include "oneapi/tbb/concurrent_unordered_map.h"

#include <map>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace phlex::experimental {

  class level_hierarchy {
    struct level_entry {
      level_entry(std::string n, std::size_t par_hash) : name{std::move(n)}, parent_hash{par_hash}
      {
      }

      std::string name;
      std::size_t parent_hash;
      std::atomic<std::size_t> count;
    };

    using levels_t = tbb::concurrent_unordered_map<std::size_t, std::shared_ptr<level_entry>>;

  public:
    level_hierarchy();
    level_hierarchy(level_hierarchy const&);
    level_hierarchy& operator=(level_hierarchy const&);

    // NOLINTNEXTLINE(cppcoreguidelines-noexcept-move-operations,performance-noexcept-move-constructor)
    level_hierarchy(level_hierarchy&&) noexcept(std::is_nothrow_move_constructible<levels_t>{});
    level_hierarchy& operator=(level_hierarchy&&) noexcept(
      std::is_nothrow_move_assignable<levels_t>{});

    ~level_hierarchy();

    void increment_count(level_id_ptr const& id);
    std::size_t count_for(std::string const& level_name) const;

    void print() const;

  private:
    std::string graph_layout() const;

    using hash_name_pair = std::pair<std::string, std::size_t>;
    using hash_name_pairs = std::vector<hash_name_pair>;
    std::string pretty_recurse(std::map<std::string, hash_name_pairs> const& tree,
                               std::string const& parent_name,
                               std::string indent = {}) const;

    levels_t levels_;
  };

}

#endif // PHLEX_MODEL_LEVEL_HIERARCHY_HPP
