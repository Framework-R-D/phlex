#ifndef PHLEX_CORE_CONSUMER_HPP
#define PHLEX_CORE_CONSUMER_HPP

#include "phlex/core/fwd.hpp"
#include "phlex/core/input_arguments.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/phlex_core_export.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace phlex::detail {
  enum class require_layers : char { multi_input_only, always };
  class PHLEX_CORE_EXPORT consumer {
    using layer_check_node_t = tbb::flow::multifunction_node<message, message_tuple<1UZ>>;

  public:
    consumer(phlex::experimental::algorithm_name name,
             std::vector<std::string> predicates,
             product_selectors input_products,
             tbb::flow::graph& graph,
             require_layers layers_required);
    virtual ~consumer();

    phlex::experimental::algorithm_name const& name() const noexcept;
    std::vector<std::string> const& when() const noexcept;

    product_selectors const& input() const noexcept;
    std::vector<phlex::experimental::identifier> const& layers() const noexcept;
    tbb::flow::receiver<message>& port(product_selector const& input_product);

    virtual named_index_ports index_ports() = 0;
    virtual std::size_t num_calls() const = 0;

  protected:
    template <typename InputParameterTuple>
    auto input_arguments()
    {
      return form_input_arguments<InputParameterTuple>(input_products_);
    }

  private:
    virtual tbb::flow::receiver<message>& port_for(product_selector const& input_product) = 0;
    phlex::experimental::algorithm_name name_;
    std::vector<std::string> predicates_;
    std::reference_wrapper<tbb::flow::graph> graph_;
    product_selectors input_products_;
    std::vector<phlex::experimental::identifier> layers_;
    std::vector<std::unique_ptr<layer_check_node_t>> layer_checkers_;
  };
}

#endif // PHLEX_CORE_CONSUMER_HPP
