#ifndef PHLEX_CONFIGURATION_HPP
#define PHLEX_CONFIGURATION_HPP

#include "boost/json.hpp"

#include <optional>
#include <utility>

namespace phlex::experimental {
  class configuration {
  public:
    configuration() = default;
    explicit configuration(boost::json::object config) : config_{std::move(config)} {}

    template <typename T>
    std::optional<T> get_if_present(std::string const& key) const
    {
      if (auto const* pkey = config_.if_contains(key)) {
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

  private:
    boost::json::object config_;
  };

}

#endif // PHLEX_CONFIGURATION_HPP
