#include "phlex/core/source.hpp"

#include <utility>

namespace phlex::experimental {
  provider_bundle::provider_bundle(provider_function_t f,
                                   concurrency c,
                                   product_specification spec,
                                   std::string layer,
                                   std::string stage) :
    provider_function_{std::move(f)},
    concurrency_{c},
    spec_{std::move(spec)},
    layer_{std::move(layer)},
    stage_{std::move(stage)}
  {
  }

  product_specification const& provider_bundle::specification() const { return spec_; }

  identifier const& provider_bundle::layer() const { return layer_; }

  std::function<provider_function_t> provider_bundle::release_provider_function()
  {
    return std::move(provider_function_);
  }

  concurrency provider_bundle::get_concurrency() const { return concurrency_; }
}
