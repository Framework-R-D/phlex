#ifndef PHLEX_CORE_DECLARED_OUTPUT_HPP
#define PHLEX_CORE_DECLARED_OUTPUT_HPP

#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/phlex_core_export.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace phlex::detail {
  namespace internal {
    using output_function_t = std::function<void(phlex::experimental::product_store const&)>;
  }
  class PHLEX_CORE_EXPORT declared_output {
  public:
    declared_output(phlex::experimental::algorithm_name name,
                    std::size_t concurrency,
                    std::vector<std::string> predicates,
                    tbb::flow::graph& g,
                    internal::output_function_t&& ft);

    phlex::experimental::algorithm_name const& name() const noexcept;
    std::vector<std::string> const& when() const noexcept;

    tbb::flow::receiver<message>& port() noexcept;
    std::size_t num_calls() const { return calls_; }

  private:
    phlex::experimental::algorithm_name name_;
    std::vector<std::string> predicates_;
    tbb::flow::function_node<message> node_;
    std::atomic<std::size_t> calls_;
  };

  using declared_output_ptr = std::unique_ptr<declared_output>;
  using declared_outputs = simple_ptr_map<declared_output_ptr>;
}

#endif // PHLEX_CORE_DECLARED_OUTPUT_HPP
