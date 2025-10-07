#ifndef phlex_core_products_consumer_hpp
#define phlex_core_products_consumer_hpp

#include "phlex/core/consumer.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/input_arguments.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/specified_label.hpp"
#include "phlex/model/algorithm_name.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <string>
#include <vector>

namespace phlex::experimental {
  class products_consumer : public consumer {
  public:
    products_consumer(algorithm_name name,
                      std::vector<std::string> predicates,
                      specified_labels input_products);

    virtual ~products_consumer();

    std::size_t num_inputs() const;

    specified_labels const& input() const noexcept;
    tbb::flow::receiver<message>& port(specified_label const& product_label);

    virtual std::vector<tbb::flow::receiver<message>*> ports() = 0;
    virtual std::size_t num_calls() const = 0;

  protected:
    template <typename InputParameterTuple>
    auto input_arguments()
    {
      return form_input_arguments<InputParameterTuple>(full_name(), input_products_);
    }

  private:
    virtual tbb::flow::receiver<message>& port_for(specified_label const& product_label) = 0;

    specified_labels input_products_;
  };
}

#endif // phlex_core_products_consumer_hpp
