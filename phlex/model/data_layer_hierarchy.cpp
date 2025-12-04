#include "phlex/model/data_layer_hierarchy.hpp"
#include "phlex/model/data_cell_index.hpp"

#include "fmt/format.h"
#include "fmt/std.h"
#include "spdlog/spdlog.h"

namespace {
  std::string const unnamed{"(unnamed)"};
  std::string const& maybe_name(std::string const& name) { return empty(name) ? unnamed : name; }
}

namespace phlex::experimental {

  data_layer_hierarchy::~data_layer_hierarchy() { print(); }

  void data_layer_hierarchy::increment_count(data_cell_index_ptr const& id)
  {
    if (auto it = layers_.find(id->layer_hash()); it != layers_.cend()) {
      ++it->second->count;
      return;
    }

    auto const parent_hash = id->has_parent() ? id->parent()->layer_hash() : -1ull;
    // Warning: It can happen that two threads get to this location at the same time.  To
    // guard against overwriting the value of "count", we use the returned iterator "it",
    // which will either refer to the new node in the map, or to the already-emplaced
    // node.  We then increment the count.
    auto [it, _] = layers_.emplace(id->layer_hash(),
                                   std::make_shared<layer_entry>(id->layer_name(), parent_hash));
    ++it->second->count;
  }

  std::size_t data_layer_hierarchy::count_for(std::string const& layer_name) const
  {
    auto it = find_if(begin(layers_), end(layers_), [&layer_name](auto const& level) {
      return level.second->name == layer_name;
    });
    return it != cend(layers_) ? it->second->count.load() : 0;
  }

  void data_layer_hierarchy::print() const { spdlog::info("{}", graph_layout()); }

  std::string data_layer_hierarchy::pretty_recurse(
    std::map<std::string, hash_name_pairs> const& tree,
    std::string const& name,
    std::string indent) const
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
      auto const& entry = *layers_.at(child_hash);
      result += "\n" + indent + " │ ";
      result += fmt::format("\n{}{}: {}", child_prefix, maybe_name(child_name), entry.count);

      auto new_indent = indent;
      new_indent += at_end ? "   " : " │ ";
      result += pretty_recurse(tree, child_name, new_indent);
    }
    return result;
  }

  std::string data_layer_hierarchy::graph_layout() const
  {
    if (empty(layers_)) {
      return {};
    }

    std::map<std::string, std::vector<hash_name_pair>> tree;
    for (auto const& [layer_hash, layer_entry] : layers_) {
      auto parent_hash = layer_entry->parent_hash;
      if (parent_hash == -1ull) {
        continue;
      }
      auto const& parent_name = layers_.at(parent_hash)->name;
      tree[parent_name].emplace_back(layer_entry->name, layer_hash);
    }

    auto const initial_indent = "  ";
    return fmt::format("\nProcessed levels:\n\n{}job{}\n",
                       initial_indent,
                       pretty_recurse(tree, "job", initial_indent));
  }
}
