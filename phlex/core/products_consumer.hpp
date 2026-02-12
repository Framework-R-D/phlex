#ifndef PHLEX_CORE_PRODUCTS_CONSUMER_HPP
#define PHLEX_CORE_PRODUCTS_CONSUMER_HPP

#include "phlex/core/consumer.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/input_arguments.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/product_query.hpp"
#include "phlex/model/algorithm_name.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <string>
#include <vector>

namespace phlex::experimental {
  class products_consumer : public consumer {
  public:
    products_consumer(algorithm_name name,
                      std::vector<std::string> predicates,
                      product_queries input_products);

    virtual ~products_consumer();

    std::size_t num_inputs() const;

    product_queries const& input() const noexcept;
    std::vector<std::string> const& layers() const noexcept;
    tbb::flow::receiver<message>& port(product_query const& product_label);

    virtual named_index_ports index_ports() = 0;
    virtual std::vector<tbb::flow::receiver<message>*> ports() = 0;
    virtual std::size_t num_calls() const = 0;

  protected:
    template <typename InputParameterTuple>
    auto input_arguments()
    {
      return form_input_arguments<InputParameterTuple>(full_name(), input_products_);
    }

  private:
    virtual tbb::flow::receiver<message>& port_for(product_query const& product_label) = 0;

    product_queries input_products_;
    std::vector<std::string> layers_;
  };
}

#endif // PHLEX_CORE_PRODUCTS_CONSUMER_HPP
