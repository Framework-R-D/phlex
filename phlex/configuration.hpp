#ifndef phlex_configuration_hpp
#define phlex_configuration_hpp

#include "boost/json.hpp"

#include <optional>

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

  private:
    boost::json::object config_;
  };

}

#endif // phlex_configuration_hpp
