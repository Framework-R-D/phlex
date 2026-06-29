#include "phlex/source.hpp"
#include "phlex/utilities/max_allowed_parallelism.hpp"

#include <cstddef>
#include <string>
#include <utility>

namespace {
  class max_parallelism_source : public phlex::source {
  public:
    phlex::detail::provider_bundles create_providers(
      phlex::product_selector const& selector) override
    {
      using namespace phlex::experimental;
      phlex::detail::provider_bundles bundles;
      std::string const layer = "job";
      std::string const stage = "CURRENT";
      product_specification spec{"input", "max_parallelism", make_type_id<std::size_t>()};

      if (selector.match(spec, identifier{layer}, identifier{stage})) {
        bundles.push_back(phlex::detail::provider_bundle{
          .provider_function =
            [](phlex::data_cell_index const&) {
              return product_for(max_allowed_parallelism::active_value());
            },
          .max_concurrency = phlex::concurrency::unlimited,
          .spec = std::move(spec),
          .layer = layer,
          .stage = stage});
      }
      return bundles;
    }

    phlex::index_generator indices() override { co_return; }
  };
}

PHLEX_REGISTER_SOURCE(s, config)
{
  s.add_source<max_parallelism_source>(config.get<std::string>("module_label"));
}
