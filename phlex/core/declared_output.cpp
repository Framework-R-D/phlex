#include "phlex/core/declared_output.hpp"
#include "phlex/configuration.hpp"

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

  output_creator::output_creator(registrar<declared_output_ptr> reg,
                                 configuration const* config,
                                 std::string name,
                                 tbb::flow::graph& g,
                                 detail::output_function_t&& f,
                                 concurrency c) :
    node_options_t{config},
    name_{config ? config->get<std::string>("module_label") : "", std::move(name)},
    graph_{g},
    ft_{std::move(f)},
    concurrency_{c},
    reg_{std::move(reg)}
  {
    reg_.set_creator([this](auto) { return create(); });
  }

}
