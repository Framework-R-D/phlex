#ifndef PHLEX_CORE_PRODUCER_CATALOG_HPP
#define PHLEX_CORE_PRODUCER_CATALOG_HPP

#include "phlex/core/message.hpp"
#include "phlex/core/product_selector.hpp"
#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/type_id.hpp"
#include "phlex/phlex_core_export.hpp"

#include <oneapi/tbb/flow_graph.h>

#include <map>
#include <ranges>
#include <span>
#include <vector>

namespace phlex::detail {
  class producer;
  using product_suffix_t = phlex::experimental::identifier;

  class PHLEX_CORE_EXPORT producer_catalog {
  public:
    explicit producer_catalog(std::span<producer* const> producers);

    struct named_output_port {
      phlex::experimental::algorithm_name node;
      tbb::flow::sender<message>* output_port;
      phlex::experimental::type_id type;
    };

    std::vector<named_output_port const*> find_producers(
      product_selector const& query,
      phlex::experimental::algorithm_name const& consumer_name,
      phlex::experimental::identifier const& stage) const;
    auto values() const { return producers_ | std::views::values; }

  private:
    std::multimap<product_suffix_t, named_output_port> producers_;
  };
}

#endif // PHLEX_CORE_PRODUCER_CATALOG_HPP
