#ifndef phlex_core_declared_output_hpp
#define phlex_core_declared_output_hpp

#include "phlex/concurrency.hpp"
#include "phlex/core/consumer.hpp"
#include "phlex/core/fwd.hpp"
#include "phlex/core/message.hpp"
#include "phlex/core/node_options.hpp"
#include "phlex/core/registrar.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/level_id.hpp"
#include "phlex/model/product_store.hpp"
#include "phlex/utilities/simple_ptr_map.hpp"

#include "oneapi/tbb/flow_graph.h"

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

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

  private:
    tbb::flow::function_node<message> node_;
  };

  using declared_output_ptr = std::unique_ptr<declared_output>;
  using declared_outputs = simple_ptr_map<declared_output_ptr>;

  class output_creator : public node_options<output_creator> {
    using node_options_t = node_options<output_creator>;

  public:
    output_creator(registrar<declared_output_ptr> reg,
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
      reg_.set([this] { return create(); });
    }

  private:
    declared_output_ptr create()
    {
      return std::make_unique<declared_output>(std::move(name_),
                                               concurrency_.value,
                                               node_options_t::release_predicates(),
                                               graph_,
                                               std::move(ft_));
    }

    algorithm_name name_;
    tbb::flow::graph& graph_;
    detail::output_function_t ft_;
    concurrency concurrency_;
    registrar<declared_output_ptr> reg_;
  };
}

#endif // phlex_core_declared_output_hpp
