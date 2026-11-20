#include "phlex/configuration.hpp"

#include "phlex/core/product_selector.hpp"
#include "phlex/model/identifier.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace phlex::detail {
  std::optional<experimental::identifier> value_if_exists(
    boost::json::object const& obj, // will be used later for new product_selector
    std::string_view parameter)
  {
    if (!obj.contains(parameter)) {
      return std::nullopt;
    }
    auto const& val = obj.at(parameter);
    return boost::json::value_to<experimental::identifier>(val);
  }

  namespace {
    std::optional<std::string_view> string_view_if_exists(
      boost::json::object const& obj, // will be used later for new product_selector
      std::string_view parameter)
    {
      if (!obj.contains(parameter)) {
        return std::nullopt;
      }
      auto const& val = obj.at(parameter);
      return boost::json::value_to<std::string_view>(val);
    }
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

  product_selector tag_invoke(boost::json::value_to_tag<product_selector> const&,
                              boost::json::value const& jv)
  {
    using detail::string_view_if_exists;
    using detail::value_if_exists;
    using namespace std::string_view_literals;
    static std::set<std::string_view> const valid_keys = {
      "creator"sv, "layer"sv, "suffix"sv, "stage"sv};
    auto query_object = jv.as_object();
    std::set<std::string_view> unrecognized_keys = {};
    for (auto const& [key, _] : query_object) {
      if (!valid_keys.contains(key)) {
        unrecognized_keys.emplace(key);
      }
    }
    if (!unrecognized_keys.empty()) {
      throw std::runtime_error(
        fmt::format("Product selector contains unrecognized keys: {}", unrecognized_keys));
    }
    auto creator = value_if_exists(query_object, "creator");
    auto layer = string_view_if_exists(query_object, "layer");
    auto suffix = value_if_exists(query_object, "suffix");
    auto stage = value_if_exists(query_object, "stage");
    return product_selector{.creator = std::move(creator),
                            .layer = layer,
                            .suffix = std::move(suffix),
                            .stage = std::move(stage)};
  }

  experimental::identifier experimental::tag_invoke(boost::json::value_to_tag<identifier> const&,
                                                    boost::json::value const& jv)
  {
    return identifier{std::string_view(jv.as_string())};
  }
}
