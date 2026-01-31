#include "phlex/configuration.hpp"
#include "phlex/core/product_query.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>

namespace {
  std::optional<phlex::experimental::identifier> value_if_exists(boost::json::object const& obj,
                                                                 std::string_view parameter)
  {
    if (!obj.contains(parameter)) {
      return std::nullopt;
    }
    auto const& val = obj.at(parameter);
    if (!val.is_string()) {
      std::string_view kind;
      switch (val.kind()) {
      case boost::json::kind::null:
        // seems reasonable to interpret this as if the value were not provided
        return std::nullopt;
        break;
      case boost::json::kind::bool_:
        kind = "bool";
        break;
      case boost::json::kind::int64:
        kind = "std::int64_t";
        break;
      case boost::json::kind::uint64:
        kind = "std::uint64_t";
        break;
      case boost::json::kind::double_:
        kind = "double";
        break;
      case boost::json::kind::array:
        kind = "array";
        break;
      case boost::json::kind::object:
        kind = "object";
        break;
      default:
        std::unreachable();
      }
      throw std::runtime_error(fmt::format(
        "Error retrieving parameter '{}'. Should be an identifier string but is instead a {}",
        parameter,
        kind));
    }
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
    auto suffix = value_if_exists(query_object, "suffix");
    auto stage = value_if_exists(query_object, "stage");
    return product_query{
      .creator = std::move(creator), .layer = std::move(layer), .suffix = suffix, .stage = stage};
  }
}
