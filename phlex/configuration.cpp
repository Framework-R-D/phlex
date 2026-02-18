#include "phlex/configuration.hpp"
#include "phlex/core/product_query.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/product_specification.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace phlex::detail {
  std::optional<phlex::experimental::identifier> value_if_exists(
    boost::json::object const& obj, // will be used later for new product_query
    std::string_view parameter)
  {
    if (!obj.contains(parameter)) {
      return std::nullopt;
    }
    auto const& val = obj.at(parameter);
    return boost::json::value_to<phlex::experimental::identifier>(val);
  }
}

namespace phlex {
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
    auto creator = value_decorate_exception<experimental::identifier>(query_object, "creator");
    auto layer = value_decorate_exception<experimental::identifier>(query_object, "layer");
    auto suffix = detail::value_if_exists(query_object, "suffix");
    auto stage = detail::value_if_exists(query_object, "stage");
    return product_query{
      .creator = std::move(creator), .layer = std::move(layer), .suffix = suffix, .stage = stage};
  }

  experimental::identifier experimental::tag_invoke(boost::json::value_to_tag<identifier> const&,
                                                    boost::json::value const& jv)
  {
    return identifier{std::string_view(jv.as_string())};
  }
}
