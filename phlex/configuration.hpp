#ifndef PHLEX_CONFIGURATION_HPP
#define PHLEX_CONFIGURATION_HPP

#include "boost/json.hpp"
#include "phlex/core/product_query.hpp"
#include "phlex/model/identifier.hpp"

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace phlex {

  namespace detail {
    template <typename T>
    T value_decorate_exception(boost::json::object const& obj, std::string const& key)
    try {
      return value_to<T>(obj.at(key));
    } catch (std::exception const& e) {
      throw std::runtime_error("Error retrieving parameter '" + key + "':\n" + e.what());
    }

    // Used later for product_query
    std::optional<phlex::experimental::identifier> value_if_exists(boost::json::object const& obj,
                                                                   std::string_view parameter);

    // helper for unpacking json array
    template <typename T, std::size_t... I>
    std::array<T, sizeof...(I)> unpack_json_array(boost::json::array const& array,
                                                  std::index_sequence<I...>)
    {
      return std::array<T, sizeof...(I)>{boost::json::value_to<T>(array.at(I))...};
    }
  }

  class configuration {
  public:
    configuration() = default;
    explicit configuration(boost::json::object const& config) : config_{config} {}

    template <typename T>
    std::optional<T> get_if_present(std::string const& key) const
    {
      if (config_.contains(key)) {
        return detail::value_decorate_exception<T>(config_, key);
      }
      return std::nullopt;
    }

    template <typename T>
    T get(std::string const& key) const
    {
      return detail::value_decorate_exception<T>(config_, key);
    }

    template <typename T>
    T get(std::string const& key, T&& default_value) const
    {
      return get_if_present<T>(key).value_or(std::forward<T>(default_value));
    }

    std::vector<std::string> keys() const;

    // Internal function for prototype purposes; do not use as this will change.
    std::pair<boost::json::kind, bool> prototype_internal_kind(std::string const& key) const
    {
      auto const& value = config_.at(key); // may throw

      auto k = value.kind();
      bool is_array = k == boost::json::kind::array;

      if (is_array) {
        // The current configuration interface only supports homogenous containers,
        // thus checking only the first element suffices. (This assumes arrays are
        // not nested, which is fine for now.)
        boost::json::array const& arr = value.as_array();
        k = arr.empty() ? boost::json::kind::null : arr[0].kind();
      }

      return std::make_pair(k, is_array);
    }

  private:
    boost::json::object config_;
  };

  // =================================================================================
  // To enable direct conversions from Boost JSON types to our own types, we implement
  // tag_invoke(...) function overloads, which are the customization points Boost JSON
  // provides.
  configuration tag_invoke(boost::json::value_to_tag<configuration> const&,
                           boost::json::value const& jv);

  product_query tag_invoke(boost::json::value_to_tag<product_query> const&,
                           boost::json::value const& jv);

  namespace experimental {
    identifier tag_invoke(boost::json::value_to_tag<identifier> const&,
                          boost::json::value const& jv);
  }

  template <std::size_t N>
  std::array<product_query, N> tag_invoke(
    boost::json::value_to_tag<std::array<product_query, N>> const&, boost::json::value const& jv)
  {
    auto const& array = jv.as_array();
    return detail::unpack_json_array<product_query>(array, std::make_index_sequence<N>());
  }
}

// The below is a better long term fix but it requires a Boost JSON bug (#1140) to be fixed
// namespace boost::json {
//   template <std::size_t N>
//   struct is_sequence_like<std::array<phlex::product_query, N>> : std::false_type {};
// }
#endif // PHLEX_CONFIGURATION_HPP
