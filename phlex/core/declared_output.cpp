#include "phlex/core/declared_output.hpp"

#include "phlex/core/detail/make_algorithm_name.hpp"
#include "phlex/core/message.hpp"
#include "phlex/model/algorithm_name.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace phlex::detail {
  declared_output::declared_output(phlex::experimental::algorithm_name name,
                                   std::size_t concurrency,
                                   std::vector<std::string> predicates,
                                   tbb::flow::graph& g,
                                   internal::output_function_t&& ft) :
    name_{std::move(name)},
    predicates_{std::move(predicates)},
    node_{g, concurrency, [this, f = std::move(ft)](message const& msg) -> tbb::flow::continue_msg {
            f(*msg.store);
            ++calls_;
            return {};
          }}
  {
  }

  phlex::experimental::algorithm_name const& declared_output::name() const noexcept
  {
    return name_;
  }
  std::vector<std::string> const& declared_output::when() const noexcept { return predicates_; }

  tbb::flow::receiver<message>& declared_output::port() noexcept { return node_; }
}
