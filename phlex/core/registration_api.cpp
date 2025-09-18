#include "phlex/core/registration_api.hpp"
#include "phlex/core/detail/maybe_predicates.hpp"

namespace phlex::experimental {
  output_api::output_api(registrar<declared_output_ptr> reg,
                         configuration const* config,
                         std::string name,
                         tbb::flow::graph& g,
                         detail::output_function_t&& f,
                         concurrency c) :
    name_{detail::make_algorithm_name(config, std::move(name))},
    graph_{g},
    ft_{std::move(f)},
    concurrency_{c},
    reg_{std::move(reg)}
  {
    // Predicates from the configuration always take precedence
    if (config) {
      reg_.set_predicates(detail::maybe_predicates(config));
    }
    reg_.set_creator([this](auto predicates, auto) {
      return std::make_unique<declared_output>(
        std::move(name_), concurrency_.value, std::move(predicates), graph_, std::move(ft_));
    });
  }

  void output_api::when(std::vector<std::string> predicates)
  {
    if (!reg_.has_predicates()) {
      reg_.set_predicates(std::move(predicates));
    }
  }
}
