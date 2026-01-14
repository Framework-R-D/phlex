#include "phlex/core/declared_output.hpp"
#include "phlex/configuration.hpp"
#include "phlex/core/detail/make_algorithm_name.hpp"

namespace phlex::experimental {
  declared_output::declared_output(algorithm_name name,
                                   std::size_t concurrency,
                                   std::vector<std::string> predicates,
                                   tbb::flow::graph& g,
                                   detail::output_function_t&& ft) :
    consumer{std::move(name), std::move(predicates)},
    node_{g, concurrency, [this, f = std::move(ft)](message const& msg) -> tbb::flow::continue_msg {
            f(*msg.store);
            ++calls_;
            return {};
          }}
  {
  }

  tbb::flow::receiver<message>& declared_output::port() noexcept { return node_; }
}
