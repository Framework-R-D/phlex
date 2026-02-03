#ifndef TEST_NODES_HPP
#define TEST_NODES_HPP

#include "message_types.hpp"
#include "phlex/model/data_cell_index.hpp"
#include "repeater_node.hpp"

#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <atomic>
#include <string>

namespace phlex::test {

  template <typename T>
  class provider_node : public tbb::flow::function_node<index_message, indexed_message> {
  public:
    provider_node(tbb::flow::graph& g, std::string data_layer, std::string product_name, T value) :
      tbb::flow::function_node<index_message, indexed_message>{
        g,
        tbb::flow::unlimited,
        [this, name = std::move(product_name)](index_message const& msg) -> indexed_message {
          ++calls_;
          spdlog::trace(
            "Provider for '{}' (\"{}\") received {}", name, layer_, msg.index->to_string());
          return {.msg_id = msg.msg_id,
                  .index = msg.index,
                  .data = std::make_shared<experimental::product<T>>(value_)};
        }},
      layer_{std::move(data_layer)},
      value_{value}
    {
    }

    unsigned calls() const noexcept { return calls_; }

    std::string const& layer() const noexcept { return layer_; }

  private:
    std::string const layer_;
    std::atomic<unsigned> calls_;
    T value_;
  };

  template <typename T>
  class consumer_node : public tbb::flow::function_node<indexed_message, indexed_message> {
  public:
    consumer_node(tbb::flow::graph& g, std::string consumer_name, std::string data_layer) :
      tbb::flow::function_node<indexed_message, indexed_message>{
        g,
        tbb::flow::unlimited,
        [this, consumer = std::move(consumer_name)](indexed_message const& msg) -> indexed_message {
          ++calls_;
          if (auto data = std::dynamic_pointer_cast<experimental::product<T> const>(msg.data)) {
            spdlog::trace("Consumer '{}' received '{}' (\"{}\")", consumer, data->obj, layer_);
          }
          return msg;
        }},
      layer_{std::move(data_layer)}
    {
    }

    unsigned calls() const noexcept { return calls_; }

    std::string const& layer() const noexcept { return layer_; }

  private:
    std::string const layer_;
    std::atomic<unsigned> calls_;
  };

  template <typename... Ts>
  using my_composite_node = tbb::flow::composite_node<indexed_message_tuple<sizeof...(Ts)>,
                                                      std::tuple<tbb::flow::continue_msg>>;

  template <typename T, typename U, typename... Ts>
  class multiarg_consumer_node : public my_composite_node<T, U, Ts...> {
    using base = my_composite_node<T, U, Ts...>;
    using input_t = typename base::input_ports_type;
    using output_t = typename base::output_ports_type;

    static constexpr auto n_inputs = 2 + sizeof...(Ts);
    using args_t = indexed_message_tuple<n_inputs>;

  public:
    multiarg_consumer_node(tbb::flow::graph& g,
                           std::string node_name,
                           std::vector<std::string> layer_names) :
      my_composite_node<T, U, Ts...>{g},
      join_{g,
            indexed_message_matcher<T>{},
            indexed_message_matcher<U>{},
            indexed_message_matcher<Ts>{}...},
      f_{g,
         tbb::flow::unlimited,
         [this](args_t const&) -> tbb::flow::continue_msg {
           ++calls_;
           return {};
         }},
      name_{std::move(node_name)},
      layers_{std::move(layer_names)}
    {
      // Add repeaters only if layer names are different
      repeaters_.reserve(n_inputs);
      for (std::size_t i = 0; i != n_inputs; ++i) {
        repeaters_.push_back(std::make_unique<repeater_node>(g));
      }

      auto set_ports = [this]<std::size_t... Is>(std::index_sequence<Is...>) {
        this->set_external_ports(input_t{repeaters_[Is]->data_port()...}, output_t{f_});
        // Connect repeaters to join
        (make_edge(*repeaters_[Is], input_port<Is>(join_)), ...);
        (repeaters_[Is]->set_metadata(name_, layers_[Is]), ...);
      };

      set_ports(std::make_index_sequence<2 + sizeof...(Ts)>{});
      make_edge(join_, f_);
    }

    std::vector<named_index_port> index_ports()
    {
      std::vector<named_index_port> result;
      [this]<std::size_t... Is>(auto& result, std::index_sequence<Is...>) {
        (result.emplace_back(
           layers_[Is], &repeaters_[Is]->flush_port(), &repeaters_[Is]->index_port()),
         ...);
      }(result, std::make_index_sequence<n_inputs>{});
      return result;
    }

    unsigned calls() const noexcept { return calls_; }

    std::string const& name() const noexcept { return name_; }
    std::vector<std::string> const& layers() const noexcept { return layers_; }

  private:
    std::vector<std::unique_ptr<repeater_node>> repeaters_;
    tbb::flow::join_node<args_t, tbb::flow::tag_matching> join_;
    tbb::flow::function_node<args_t> f_;
    std::string const name_;
    std::vector<std::string> const layers_;
    std::atomic<unsigned int> calls_;
  };
}

#endif // TEST_NODES_HPP
