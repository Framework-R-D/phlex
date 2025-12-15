#include "phlex/configuration.hpp"
#include "phlex/core/product_query.hpp"

#include <ranges>
#include <string>

namespace phlex::experimental {
  std::vector<std::string> configuration::keys() const
  {
    std::vector<std::string> result;
    result.reserve(config_.size());
    std::ranges::transform(
      config_, std::back_inserter(result), [](auto const& element) { return element.key(); });
    return result;
  }

  configuration tag_invoke(boost::json::value_to_tag<configuration> const&,
                           boost::json::value const& jv)
  {
    return configuration{jv.as_object()};
  }

  product_query tag_invoke(boost::json::value_to_tag<product_query> const&,
                           boost::json::value const& jv)
  {
    using detail::value_decorate_exception;
    auto query_object = jv.as_object();
    return product_query{.spec = {value_decorate_exception<std::string>(query_object, "product")},
                         .layer = value_decorate_exception<std::string>(query_object, "layer")};
  }
}
