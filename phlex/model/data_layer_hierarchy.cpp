#include "phlex/model/data_layer_hierarchy.hpp"

#include "phlex/model/fwd.hpp"
#include "phlex/model/layer_path.hpp"
#include "phlex/utilities/bulleted_list.hpp"

#include <fmt/format.h>

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
  struct display_node {
    std::map<std::string, display_node> children;
    std::optional<std::size_t> count;
  };

  std::string const& maybe_name(std::string const& name)
  {
    static std::string const unnamed{"(unnamed)"};
    return empty(name) ? unnamed : name; // NOLINT(bugprone-return-const-ref-from-parameter)
  }

  void render(display_node const& node, std::string const& indent, std::string& result)
  {
    std::size_t i = 0;
    for (auto const& [name, child] : node.children) {
      bool const last = ++i == node.children.size();
      auto const count = child.count ? fmt::to_string(*child.count) : "(incomplete)";
      result += fmt::format(
        "\n{} │\n{} {} {}: {}", indent, indent, last ? "└" : "├", maybe_name(name), count);
      render(child, indent + (last ? "   " : " │ "), result);
    }
  }
}

namespace phlex::detail {

  data_layer_hierarchy::~data_layer_hierarchy() = default;

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
    auto [it, _] = layers_.emplace(
      id->layer_hash(),
      std::make_shared<layer_entry>(id->layer_name(), id->layer_path(), parent_hash));
    ++it->second->count;
  }

  std::size_t data_layer_hierarchy::count_for(phlex::experimental::layer_path const& layer,
                                              bool const missing_ok) const
  {
    // The assumption is that specified layer is a portion of a layer path
    // sufficient to uniquely identify a layer
    std::vector<layer_entry const*> candidates;
    for (auto const& [_, entry] : layers_) {
      if (entry->layer_path.ends_with(layer)) {
        candidates.push_back(entry.get());
      }
    }

    if (candidates.empty()) {
      return missing_ok ? 0ull
                        : throw std::runtime_error(
                            fmt::format("No layers match the specification {}", layer));
    }

    if (candidates.size() > 1ull) {
      std::string const msg =
        fmt::format("The following data layers match the specification {}:\n\n{}"
                    "\n\nPlease specify the full layer path to disambiguate between them.",
                    layer,
                    bulleted_list(candidates | std::views::transform([](auto const* entry) {
                                    return entry->layer_path;
                                  }),
                                  /*indent=*/0));
      throw std::runtime_error(msg);
    }

    return candidates[0]->count.load();
  }

  std::string data_layer_hierarchy::to_string() const
  {
    if (empty(layers_)) {
      return {};
    }

    display_node root;
    for (auto const& [_, entry] : layers_) {
      auto* node = &root;
      for (auto const& component : entry->layer_path.components().subspan(1)) {
        node = &node->children[fmt::to_string(component)];
      }
      node->count = entry->count.load();
    }

    auto result = fmt::format("\n\nSeen layers:\n\n  job{}", root.count ? "" : ": (incomplete)");
    render(root, "  ", result);
    result += '\n';
    return result;
  }
}
