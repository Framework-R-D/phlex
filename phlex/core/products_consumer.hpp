#ifndef phlex_core_products_consumer_hpp
#define phlex_core_products_consumer_hpp

#include "phlex/core/consumer.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/specified_label.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/level_id.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <span>
#include <string>
#include <vector>

namespace phlex::experimental {
  class products_consumer : public consumer {
  public:
    products_consumer(algorithm_name name, std::vector<std::string> predicates);

    virtual ~products_consumer();

    std::size_t num_inputs() const;

    tbb::flow::receiver<message>& port(specified_label const& product_label);
    virtual std::vector<tbb::flow::receiver<message>*> ports() = 0;
    virtual specified_labels input() const = 0;
    virtual std::size_t num_calls() const = 0;

  private:
    virtual tbb::flow::receiver<message>& port_for(specified_label const& product_label) = 0;
  };
}

#endif // phlex_core_products_consumer_hpp
