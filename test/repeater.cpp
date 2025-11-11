// =============================  //
// This test is used to determine whether "repeaters" can work. //
//                                //
//     (1) input                  //
//           |                    //
//     (2) router                 //
//        /      \                //
//       |        \               //
//       |         \              //
// (3) I(run)      I(spill) (4)   //
//       |         /|             //
//       |        / |             //
// (5) expo      /  |             //
//       |      /   |             //
//        \    /    |             //
//  (7) repeater   number (6)     //
//          \      /              //
//           \    /               //
//       (8) consume              //
// ============================== //

#include "catch2/catch_test_macros.hpp"
#include "fmt/ranges.h"
#include "fmt/std.h"
#include "oneapi/tbb/concurrent_hash_map.h"
#include "oneapi/tbb/concurrent_queue.h"
#include "oneapi/tbb/flow_graph.h"
#include "spdlog/spdlog.h"

#include <cassert>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

using namespace oneapi::tbb;
using namespace spdlog;

namespace {

  constexpr int num_runs = 5;
  constexpr int messages_per_run = 10;
  constexpr int num_messages = num_runs * messages_per_run;

  constexpr int spills_per_run = messages_per_run - 1;
  constexpr int num_spills = num_runs * spills_per_run;

  using id_t = std::vector<int>;
  using product_t = double;
  using product_ptr_t = std::shared_ptr<product_t>;

  struct data_cell_id {
    int msg_id;
    id_t cell_id;
  };

  struct message {
    data_cell_id id;
    product_ptr_t product;
  };

  using repeater_node_input = std::tuple<message, data_cell_id>;
  class repeater_node : public flow::composite_node<repeater_node_input, std::tuple<message>> {
    using base_t = flow::composite_node<repeater_node_input, std::tuple<message>>;
    using tagged_repeater_msg = flow::tagged_msg<std::size_t, message, data_cell_id>;

    struct cached_product {
      product_ptr_t product{};
      concurrent_queue<int> msg_ids{};
    };

    using cached_products_t = concurrent_hash_map<int, cached_product>;
    using accessor = cached_products_t::accessor;
    using const_accessor = cached_products_t::const_accessor;

  public:
    repeater_node(flow::graph& g) :
      base_t{g},
      indexer_{g},
      repeater_{g,
                flow::unlimited,
                [this](tagged_repeater_msg const& tagged, auto& outputs) -> flow::continue_msg {
                  int key = -1;
                  if (tagged.tag() == 0ull) {
                    auto const& msg = tagged.cast_to<message>();
                    key = msg.id.cell_id[0];
                    accessor a;
                    std::ignore = cached_products_.insert(a, key);
                    a->second.product = msg.product;
                  } else {
                    auto const [msg_id, cell_id] = tagged.cast_to<data_cell_id>();
                    key = cell_id[0];
                    accessor a;
                    std::ignore = cached_products_.insert(a, key);
                    a->second.msg_ids.push(msg_id);
                  }

                  accessor ca;
                  bool const result [[maybe_unused]] = cached_products_.find(ca, key);
                  assert(result);
                  int new_msg_id{};
                  auto& output = std::get<0>(outputs);
                  while (ca->second.product and ca->second.msg_ids.try_pop(new_msg_id)) {
                    output.try_put({.id = data_cell_id{.msg_id = new_msg_id, .cell_id = {key}},
                                    .product = ca->second.product});
                  }
                  return {};
                }}
    {
      base_t::set_external_ports(
        base_t::input_ports_type{input_port<0>(indexer_), input_port<1>(indexer_)},
        base_t::output_ports_type{output_port<0>(repeater_)});
      make_edge(indexer_, repeater_);
    }

  private:
    flow::indexer_node<message, data_cell_id> indexer_;
    flow::multifunction_node<tagged_repeater_msg, std::tuple<message>> repeater_;
    concurrent_hash_map<int, cached_product> cached_products_; // FIXME: int should be the ID itself
  };

}

TEST_CASE("Serialize functions based on resource", "[multithreading]")
{
  spdlog::flush_on(spdlog::level::trace);

  flow::graph g;

  // 1. input
  int i{};
  flow::input_node src{g, [&i](flow_control& fc) -> data_cell_id {
                         if (i == num_messages) {
                           fc.stop();
                           return {};
                         }

                         auto const [remainder, quotient] = std::div(i++, 10);
                         if (quotient == 0) {
                           return {.msg_id = i, .cell_id = {remainder}};
                         }
                         return {.msg_id = i, .cell_id = {remainder, quotient}};
                       }};

  flow::broadcast_node<data_cell_id> run_index_set{g};   // 3. I(run)
  flow::broadcast_node<data_cell_id> spill_index_set{g}; // 4. I(spill)

  // 2. router
  flow::function_node<data_cell_id> router{
    g,
    flow::unlimited,
    [&run_index_set, &spill_index_set](data_cell_id const& id) -> flow::continue_msg {
      auto const& [_, cell_id] = id;
      if (cell_id.size() == 1ull) {
        run_index_set.try_put(id);
      } else {
        spill_index_set.try_put(id);
      }
      return {};
    }};
  make_edge(src, router);

  // 5. exponent provider
  std::atomic<int> run_counter;
  flow::function_node<data_cell_id, message> exponent_provider{
    g, flow::unlimited, [&run_counter](data_cell_id const& id) -> message {
      ++run_counter;
      return {.id = id, .product = std::make_shared<double>(id.cell_id[0])};
    }};
  make_edge(run_index_set, exponent_provider);

  // 6. number provider
  flow::function_node<data_cell_id, message> number_provider{
    g, flow::unlimited, [](data_cell_id const& id) -> message {
      return {.id = id, .product = std::make_shared<double>(id.cell_id[1])};
    }};
  make_edge(spill_index_set, number_provider);

  // 7. repeater
  repeater_node repeater{g};
  make_edge(exponent_provider, input_port<0>(repeater));
  make_edge(spill_index_set, input_port<1>(repeater));

  auto use_message_id = [](message const& msg) { return msg.id.msg_id; };
  flow::join_node<std::tuple<message, message>, flow::tag_matching> join_layers{
    g, use_message_id, use_message_id};
  make_edge(repeater, input_port<0>(join_layers));
  make_edge(number_provider, input_port<1>(join_layers));

  // 8. consume
  std::atomic<int> spill_counter;
  flow::function_node<std::tuple<message, message>> consume{
    g,
    flow::unlimited,
    [&spill_counter](std::tuple<message, message> const& joined_data) -> flow::continue_msg {
      auto const& [a, b] = joined_data;
      assert(a.id.cell_id[0] == b.id.cell_id[0]);
      ++spill_counter;
      return {};
    }};
  make_edge(join_layers, consume);

  src.activate();
  g.wait_for_all();

  CHECK(run_counter == num_runs);
  CHECK(spill_counter == num_spills);
}
