#ifndef TEST_INDEX_ROUTER_HPP
#define TEST_INDEX_ROUTER_HPP

#include "message_types.hpp"
#include "phlex/model/data_cell_index.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <map>
#include <string>
#include <unordered_map>

namespace phlex::test {
  using layers_t = std::vector<std::string>;

  class index_router {
  public:
    using flush_node = tbb::flow::broadcast_node<indexed_end_token>;
    using index_set_node = tbb::flow::broadcast_node<index_message>;

    explicit index_router(tbb::flow::graph& g,
                          std::vector<std::string> layers,
                          std::map<std::string, named_index_ports> multilayers);
    auto index_node_for(std::string const& layer) -> index_set_node&;

    void shutdown();
    void route(data_cell_index_ptr const& index);

  private:
    void send(data_cell_index_ptr const& index, std::size_t message_id);
    void multisend(data_cell_index_ptr const& index, std::size_t message_id);
    void backout_to(data_cell_index_ptr const& index);

    using broadcasters_t = std::map<std::string, index_set_node>;
    broadcasters_t broadcasters_;

    struct multibroadcaster_entry {
      std::string layer;
      index_set_node broadcaster;
      flush_node flusher;
    };
    using multibroadcaster_entries = std::vector<multibroadcaster_entry>;

    using multibroadcasters_t = std::unordered_map<std::string, multibroadcaster_entries>;
    multibroadcasters_t multibroadcasters_;

    class multilayer_sender {
    public:
      multilayer_sender(std::string const& layer, index_set_node* broadcaster, flush_node* flusher);

      auto const& layer() const noexcept { return layer_; }
      void put_message(data_cell_index_ptr const& index, std::size_t message_id);
      void put_end_token(data_cell_index_ptr const& index);

    private:
      std::string layer_;
      index_set_node* broadcaster_;
      flush_node* flusher_;
      int counter_ = 0;
    };

    using cached_casters_t = std::unordered_map<std::size_t, std::vector<multilayer_sender>>;
    cached_casters_t cached_multicasters_;
    std::atomic<unsigned> counter_;
    data_cell_index_ptr last_index_;
  };
}

#endif // TEST_INDEX_ROUTER_HPP
