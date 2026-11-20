#include "phlex/configuration.hpp"

#include "phlex/core/product_selector.hpp"
#include "phlex/model/identifier.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace phlex::experimental {
  // The unnamed namespace gives this helper internal linkage. It is inline so that its members
  // are still found by argument-dependent lookup in phlex::experimental.
  inline namespace {
    // Boost.JSON conversion to suffix_t, used when reading a product selector's "suffix" field.
    // boost::json::value_to finds tag_invoke overloads by argument-dependent lookup, which
    // searches phlex::experimental, the namespace of suffix_t, and its inline namespaces.
    suffix_t tag_invoke(boost::json::value_to_tag<suffix_t> const&, boost::json::value const& jv)
    {
      return suffix_t{std::string_view{jv.as_string()}};
    }
  }
}

namespace phlex {
  namespace {
    // Returns the product selector field named `field` converted to T, or std::nullopt when it is
    // absent. A malformed value is reported together with the name of the field that held it.
    template <typename T>
    std::optional<T> in_selector_field(boost::json::object const& query_object,
                                       std::string_view field)
    try {
      return detail::value_if_exists<T>(query_object, field);
    } catch (std::invalid_argument const& e) {
      throw std::invalid_argument{
        fmt::format("Invalid product selector field \"{}\": {}", field, e.what())};
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
    using experimental::identifier;
    using experimental::suffix_t;
    using namespace std::string_view_literals;
    static std::set<std::string_view> const valid_keys = {
      "creator"sv, "layer"sv, "suffix"sv, "stage"sv};
    // A reference into `jv`: the selector is only read here, so no copy of the object is needed.
    auto const& query_object = jv.as_object();
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
    // Each field is initialized directly from the value returned by in_selector_field, so no
    // field value is copied or moved through a named local. The initializers of a braced list are
    // evaluated in order, so fields are read in declaration order and the first malformed field
    // in that order is the one reported.
    return product_selector{.creator = in_selector_field<identifier>(query_object, "creator"),
                            .layer = in_selector_field<identifier>(query_object, "layer"),
                            .suffix = in_selector_field<suffix_t>(query_object, "suffix"),
                            .stage = in_selector_field<identifier>(query_object, "stage")};
  }

  experimental::identifier experimental::tag_invoke(boost::json::value_to_tag<identifier> const&,
                                                    boost::json::value const& jv)
  {
    return identifier{std::string_view(jv.as_string())};
  }
}
