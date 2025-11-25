#include "phlex/core/declared_output.hpp"
#include "phlex/concurrency.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/consumer.hpp"
#include "phlex/core/detail/make_algorithm_name.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/registrar.hpp"
#include <cstddef>
#include <oneapi/tbb/flow_graph.h>
#include <string>
#include <utility>
#include <vector>

namespace phlex::experimental {
  declared_output::declared_output(algorithm_name name,
                                   std::size_t concurrency,
                                   std::vector<std::string> predicates,
                                   tbb::flow::graph& g,
                                   detail::output_function_t&& ft) :
    consumer{std::move(name), std::move(predicates)},
    node_{g, concurrency, [f = std::move(ft)](message const& msg) -> tbb::flow::continue_msg {
            if (not msg.store->is_flush()) {
              f(*msg.store);
            }
            return {};
          }}
  {
  }

  tbb::flow::receiver<message>& declared_output::port() noexcept { return node_; }
}
