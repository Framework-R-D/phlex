#include "phlex/model/level_hierarchy.hpp"
#include "phlex/model/fwd.hpp"

#include "spdlog/spdlog.h"
#include <algorithm>
#include <cstddef>
#include <fmt/core.h>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace {
  std::string const unnamed{"(unnamed)"};
  auto maybe_name(std::string const& name) -> std::string const& { return empty(name) ? unnamed : name; }
}

namespace phlex::experimental {

  level_hierarchy::~level_hierarchy() { print(); }

  void level_hierarchy::increment_count(level_id_ptr const& id)
  {
    if (auto it = levels_.find(id->level_hash()); it != levels_.cend()) {
      ++it->second->count;
      return;
    }

    auto const parent_hash = id->has_parent() ? id->parent()->level_hash() : -1ull;
    // Warning: It can happen that two threads get to this location at the same time.  To
    // guard against overwriting the value of "count", we use the returned iterator "it",
    // which will either refer to the new node in the map, or to the already-emplaced
    // node.  We then increment the count.
    auto [it, _] = levels_.emplace(id->level_hash(),
                                   std::make_shared<level_entry>(id->level_name(), parent_hash));
    ++it->second->count;
  }

  auto level_hierarchy::count_for(std::string const& level_name) const -> std::size_t
  {
    auto it = std::ranges::find_if(levels_, [&level_name](auto const& level) -> auto {
      return level.second->name == level_name;
    });
    return it != cend(levels_) ? it->second->count.load() : 0;
  }

  void level_hierarchy::print() const { spdlog::info("{}", graph_layout()); }

  auto level_hierarchy::pretty_recurse(std::map<std::string, hash_name_pairs> const& tree,
                                              std::string const& name,
                                              std::string indent) const -> std::string
  {
    auto it = tree.find(name);
    if (it == cend(tree)) {
      return {};
    }

    std::string result;
    std::size_t const n = it->second.size();
    for (std::size_t i = 0; auto const& [child_name, child_hash] : it->second) {
      bool const at_end = ++i == n;
      auto child_prefix = !at_end ? indent + " ├ " : indent + " └ ";
      auto const& entry = *levels_.at(child_hash);
      result += "\n" + indent + " │ ";
      result += fmt::format("\n{}{}: {}", child_prefix, maybe_name(child_name), entry.count);

      auto new_indent = indent;
      new_indent += at_end ? "   " : " │ ";
      result += pretty_recurse(tree, child_name, new_indent);
    }
    return result;
  }

  auto level_hierarchy::graph_layout() const -> std::string
  {
    if (empty(levels_)) {
      return {};
    }

    std::map<std::string, std::vector<hash_name_pair>> tree;
    for (auto const& [level_hash, level_entry] : levels_) {
      auto parent_hash = level_entry->parent_hash;
      if (parent_hash == -1ull) {
        continue;
      }
      auto const& parent_name = levels_.at(parent_hash)->name;
      tree[parent_name].emplace_back(level_entry->name, level_hash);
    }

    auto const initial_indent = "  ";
    return fmt::format("\nProcessed levels:\n\n{}job{}\n",
                       initial_indent,
                       pretty_recurse(tree, "job", initial_indent));
  }
}
