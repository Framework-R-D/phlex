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
}

namespace phlex {
  namespace {
    // Reports a malformed value together with the name of the product selector field that held it.
    template <typename Convert>
    auto in_selector_field(std::string_view field, Convert&& convert)
    {
      try {
        return std::forward<Convert>(convert)();
      } catch (std::invalid_argument const& e) {
        throw std::invalid_argument{
          fmt::format("Invalid product selector field \"{}\": {}", field, e.what())};
      }
    }
  }

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
    auto creator =
      in_selector_field("creator", [&] { return value_if_exists(query_object, "creator"); });
    auto layer = in_selector_field("layer", [&] { return value_if_exists(query_object, "layer"); });
    std::optional<experimental::suffix_t> suffix;
    if (query_object.contains("suffix")) {
      suffix = in_selector_field("suffix", [&] {
        return experimental::suffix_t{std::string_view{query_object.at("suffix").as_string()}};
      });
    }
    auto stage = in_selector_field("stage", [&] { return value_if_exists(query_object, "stage"); });
    return product_selector{.creator = std::move(creator),
                            .layer = std::move(layer),
                            .suffix = std::move(suffix),
                            .stage = stage};
  }

  experimental::identifier experimental::tag_invoke(boost::json::value_to_tag<identifier> const&,
                                                    boost::json::value const& jv)
  {
    return identifier{std::string_view(jv.as_string())};
  }
}
