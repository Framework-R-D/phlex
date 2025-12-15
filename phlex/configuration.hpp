#ifndef PHLEX_CONFIGURATION_HPP
#define PHLEX_CONFIGURATION_HPP

#include "boost/json.hpp"
#include "phlex/core/product_query.hpp"

#include <optional>
#include <string>
#include <vector>

namespace phlex::experimental {
  class configuration {
  public:
    configuration() = default;
    explicit configuration(boost::json::object const& config) : config_{config} {}

    template <typename T>
    std::optional<T> get_if_present(std::string const& key) const
    {
      if (auto pkey = config_.if_contains(key)) {
        return value_to<T>(*pkey);
      }
      return std::nullopt;
    }

    template <typename T>
    T get(std::string const& key) const
    try {
      return value_to<T>(config_.at(key));
    } catch (std::exception const& e) {
      throw std::runtime_error("Error retrieving parameter '" + key + "':\n" + e.what());
    }

    template <typename T>
    T get(std::string const& key, T&& default_value) const
    {
      return get_if_present<T>(key).value_or(std::forward<T>(default_value));
    }

    std::vector<std::string> keys() const;

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
}

#endif // PHLEX_CONFIGURATION_HPP
