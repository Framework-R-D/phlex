#ifndef PHLEX_CORE_FILTER_HPP
#define PHLEX_CORE_FILTER_HPP

#include "phlex/core/detail/filter_impl.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"

#include "oneapi/tbb/flow_graph.h"

namespace phlex::experimental {
  using filter_base =
    oneapi::tbb::flow::composite_node<std::tuple<message, predicate_result>,
                                      std::tuple<oneapi::tbb::flow::continue_msg>>;

  class filter : public filter_base {
    using indexer_t = oneapi::tbb::flow::indexer_node<message, predicate_result>;
    using tag_t = indexer_t::output_type;

  public:
    using filter_base::input_ports_type;
    using filter_base::output_ports_type;

    explicit filter(oneapi::tbb::flow::graph& g, products_consumer& consumer);
    explicit filter(oneapi::tbb::flow::graph& g, declared_output& output);

    auto& data_port() { return input_port<0>(*this); }
    auto& predicate_port() { return input_port<1>(*this); }

  private:
    oneapi::tbb::flow::continue_msg execute(tag_t const& tag);

    decision_map decisions_;
    data_map data_;
    indexer_t indexer_;
    oneapi::tbb::flow::function_node<tag_t> filter_;
    std::vector<oneapi::tbb::flow::receiver<message>*> downstream_ports_;
    std::size_t nargs_;
  };
}

#endif // PHLEX_CORE_FILTER_HPP
