#ifndef PHLEX_CONFIGURATION_HPP
#define PHLEX_CONFIGURATION_HPP

#include "boost/json.hpp"

#include <optional>
#include <utility>

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
    {
      return value_to<T>(config_.at(key));
    }

    template <typename T>
    T get(std::string const& key, T&& default_value) const
    {
      return get_if_present<T>(key).value_or(std::forward<T>(default_value));
    }

    // Internal function for prototype purposes; do not use as this will change.
    std::pair<boost::json::kind, bool> kind(std::string const& key) const
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

}

#endif // PHLEX_CONFIGURATION_HPP
