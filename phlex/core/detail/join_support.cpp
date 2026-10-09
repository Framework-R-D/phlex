#include "phlex/core/detail/join_support.hpp"

#include <set>
#include <utility>

namespace phlex::detail::internal {

  join_support::join_support(tbb::flow::graph& g,
                             std::string const& node_name,
                             std::vector<phlex::experimental::identifier> const& layers)
  {
    if (std::set(layers.begin(), layers.end()).size() <= 1) {
      return;
    }
    // Same-layer joins never parsed the node name because they allocated no repeaters.
    initialize(g, node_name, layers, std::nullopt);
  }

  join_support::join_support(tbb::flow::graph& g,
                             phlex::experimental::algorithm_name const& node_name,
                             std::vector<phlex::experimental::identifier> const& layers,
                             named_index_port partition)
  {
    initialize(g, node_name, layers, std::move(partition));
  }

  void join_support::initialize(tbb::flow::graph& g,
                                phlex::experimental::algorithm_name const& node_name,
                                std::vector<phlex::experimental::identifier> const& layers,
                                std::optional<named_index_port> partition)
  {
    repeaters_.reserve(layers.size());
    index_ports_.reserve(layers.size() + (partition ? 1 : 0));
    if (partition) {
      // Preserve the fold's current first-input counting-layer choice.
      partition->counting_layer = layers.empty() ? partition->layer : layers.front();
      index_ports_.push_back(*partition);
    }
    for (auto const& layer : layers) {
      auto& repeater =
        *repeaters_.emplace_back(std::make_unique<repeater_node>(g, node_name, layer));
      index_ports_.emplace_back(layer,
                                partition ? std::optional{layer} : std::nullopt,
                                &repeater.flush_port(),
                                &repeater.index_port());
    }
  }

  join_support::~join_support() = default;

  bool join_support::has_repeaters() const noexcept { return !repeaters_.empty(); }

  repeater_node& join_support::repeater(std::size_t index) { return *repeaters_.at(index); }

  named_index_ports const& join_support::index_ports() const noexcept { return index_ports_; }
}
