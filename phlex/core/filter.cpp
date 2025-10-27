#include "phlex/core/filter.hpp"
#include "phlex/core/declared_output.hpp"
#include "phlex/core/detail/filter_impl.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/products_consumer.hpp"

#include "oneapi/tbb/flow_graph.h"
#include <cstddef>
#include <iterator>

using namespace phlex::experimental;
using namespace oneapi::tbb;

namespace phlex::experimental {
  filter::filter(flow::graph& g, products_consumer& consumer) :
    filter_base{g},
    decisions_{static_cast<unsigned int>(consumer.when().size())},
    data_{consumer.input()},
    indexer_{g},
    filter_{g, flow::unlimited, [this](tag_t const& t) -> oneapi::tbb::flow::continue_msg { return execute(t); }},
    downstream_ports_{consumer.ports()},
    nargs_{size(downstream_ports_)}
  {
    make_edge(indexer_, filter_);
    set_external_ports(input_ports_type{input_port<0>(indexer_), input_port<1>(indexer_)},
                       output_ports_type{filter_});
  }

  filter::filter(flow::graph& g, declared_output& output) :
    filter_base{g},
    decisions_{static_cast<unsigned int>(output.when().size())},
    data_{data_map::for_output},
    indexer_{g},
    filter_{g, flow::unlimited, [this](tag_t const& t) -> oneapi::tbb::flow::continue_msg { return execute(t); }},
    downstream_ports_{&output.port()},
    nargs_{size(downstream_ports_)}
  {
    make_edge(indexer_, filter_);
    set_external_ports(input_ports_type{input_port<0>(indexer_), input_port<1>(indexer_)},
                       output_ports_type{filter_});
  }

  auto filter::execute(tag_t const& t) -> flow::continue_msg
  {
    // FIXME: This implementation is horrible!  Because there are two data structures that
    //        have to work together.
    end_of_message_ptr eom{};
    unsigned int msg_id{};
    if (t.is_a<message>()) {
      auto const& msg = t.cast_to<message>();
      if (msg.store->is_flush()) {
        // All flush messages are automatically forwarded to downstream ports.
        for (std::size_t i = 0ull; i != nargs_; ++i) {
          downstream_ports_[i]->try_put(msg);
        }
        return {};
      }
      msg_id = msg.id;
      data_.update(msg.id, msg.store);
    } else {
      auto const& result = t.cast_to<predicate_result>();
      decisions_.update(result);
      eom = result.eom;
      msg_id = result.msg_id;
    }

    auto const filter_decision = decisions_.value(msg_id);
    if (not is_complete(filter_decision)) {
      return {};
    }

    if (not data_.is_complete(msg_id)) {
      return {};
    }

    if (decision_map::accessor a; to_boolean(filter_decision) && decisions_.claim(a, msg_id)) {
      auto const stores = data_.release_data(msg_id);
      if (empty(stores)) {
        return {};
      }
      for (std::size_t i = 0ull; i != nargs_; ++i) {
        downstream_ports_[i]->try_put({.store=stores[i], .eom=eom, .id=msg_id});
      }
      // Decision must be erased while access is claimed
      decisions_.erase(a);
    }
    return {};
  }
}
