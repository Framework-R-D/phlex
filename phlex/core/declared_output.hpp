#ifndef PHLEX_CORE_DECLARED_OUTPUT_HPP
#define PHLEX_CORE_DECLARED_OUTPUT_HPP

#include "phlex/core/consumer.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace phlex::experimental {
  namespace detail {
    using output_function_t = std::function<void(product_store const&)>;
  }
  class declared_output : public consumer {
  public:
    declared_output(algorithm_name name,
                    std::size_t concurrency,
                    std::vector<std::string> predicates,
                    tbb::flow::graph& g,
                    detail::output_function_t&& ft);

    tbb::flow::receiver<message>& port() noexcept;
    std::size_t num_calls() const { return calls_; }

  private:
    tbb::flow::function_node<message> node_;
    std::atomic<std::size_t> calls_;
  };

  using declared_output_ptr = std::unique_ptr<declared_output>;
  using declared_outputs = simple_ptr_map<declared_output_ptr>;
}

#endif // PHLEX_CORE_DECLARED_OUTPUT_HPP
