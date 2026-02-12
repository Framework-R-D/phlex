#ifndef PHLEX_CORE_MULTIPLEXER_HPP
#define PHLEX_CORE_MULTIPLEXER_HPP

#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"
#include "phlex/model/data_cell_counter.hpp"
#include "phlex/model/data_cell_index.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <stack>
#include <string>
#include <unordered_map>
#include <utility>

namespace phlex::experimental {
  namespace detail {
    using index_set_node = tbb::flow::broadcast_node<index_message>;
    using flush_node = tbb::flow::broadcast_node<indexed_end_token>;

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

    using multilayer_sender_ptrs_t = std::vector<std::shared_ptr<multilayer_sender>>;
    using cached_casters_t = std::unordered_map<std::size_t, multilayer_sender_ptrs_t>;
  }

  class layer_sentry {
  public:
    layer_sentry(flush_counters& counters,
                 flusher_t& flusher,
                 detail::multilayer_sender_ptrs_t const& senders_for_layer,
                 data_cell_index_ptr index,
                 std::size_t message_id);
    ~layer_sentry();
    std::size_t depth() const;

  private:
    flush_counters& counters_;
    flusher_t& flusher_;
    detail::multilayer_sender_ptrs_t const& senders_;
    data_cell_index_ptr index_;
    std::size_t message_id_;
  };

  class multiplexer {
  public:
    struct named_input_port {
      product_query product_label;
      tbb::flow::receiver<message>* port;
    };
    using named_input_ports_t = std::vector<named_input_port>;

    // map of node name to its input ports
    using head_ports_t = std::map<std::string, named_input_ports_t>;

    struct provider_input_port_t {
      product_query product_label;
      tbb::flow::receiver<index_message>* port;
    };
    using provider_input_ports_t = std::map<std::string, provider_input_port_t>;

    explicit multiplexer(tbb::flow::graph& g);
    data_cell_index_ptr route(data_cell_index_ptr index);

    void finalize(tbb::flow::graph& g,
                  provider_input_ports_t provider_input_ports,
                  std::map<std::string, named_index_ports> multilayers);
    void drain();
    flusher_t& flusher() { return flusher_; }

  private:
    void backout_to(data_cell_index_ptr store);
    void send_to_provider_index_nodes(data_cell_index_ptr const& index, std::size_t message_id);
    detail::multilayer_sender_ptrs_t const& send_to_multilayer_join_nodes(
      data_cell_index_ptr const& index, std::size_t message_id);
    detail::index_set_node* index_node_for(std::string const& layer);

    struct multibroadcaster_entry {
      std::string layer;
      detail::index_set_node broadcaster;
      detail::flush_node flusher;
    };
    using multibroadcaster_entries = std::vector<multibroadcaster_entry>;

    provider_input_ports_t provider_input_ports_;
    std::atomic<std::size_t> received_indices_{};
    flusher_t flusher_;
    flush_counters counters_;
    std::stack<layer_sentry> layers_;

    using broadcasters_t = std::unordered_map<std::string, detail::index_set_node>;
    broadcasters_t broadcasters_;

    using multibroadcasters_t = std::unordered_map<std::string, multibroadcaster_entries>;
    multibroadcasters_t multibroadcasters_;

    std::unordered_map<std::size_t, detail::index_set_node*> cached_broadcasters_;
    detail::cached_casters_t cached_multicasters_;

    std::unordered_map<std::size_t, detail::multilayer_sender_ptrs_t> cached_casters_for_flushing_;
  };

}

#endif // PHLEX_CORE_MULTIPLEXER_HPP
