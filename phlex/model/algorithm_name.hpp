#ifndef PHLEX_MODEL_ALGORITHM_NAME_HPP
#define PHLEX_MODEL_ALGORITHM_NAME_HPP

#include "phlex_model_export.hpp"

#include <string>

namespace phlex::experimental {
  class phlex_model_EXPORT algorithm_name {
    enum specified_fields { neither, either, both };

  public:
    algorithm_name();

    algorithm_name(char const* spec);
    algorithm_name(std::string spec);
    algorithm_name(std::string plugin,
                   std::string algorithm,
                   specified_fields fields = specified_fields::both);

    std::string full() const;
    std::string const& plugin() const noexcept { return plugin_; }
    std::string const& algorithm() const noexcept { return algorithm_; }

    bool match(algorithm_name const& other) const;
    auto operator<=>(algorithm_name const&) const = default;

    static algorithm_name create(char const* spec);
    static algorithm_name create(std::string const& spec);

  private:
    auto cmp_tuple() const;
    std::string plugin_;
    std::string algorithm_;
    specified_fields fields_{specified_fields::neither};
  };

}

#endif // PHLEX_MODEL_ALGORITHM_NAME_HPP
