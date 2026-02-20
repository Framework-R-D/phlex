#ifndef PHLEX_MODEL_ALGORITHM_NAME_HPP
#define PHLEX_MODEL_ALGORITHM_NAME_HPP

#include "phlex/model/identifier.hpp"

namespace phlex::experimental {
  class algorithm_name {
    enum class specified_fields { neither, either, both };

  public:
    algorithm_name();

    algorithm_name(char const* spec);
    algorithm_name(std::string const& spec);
    algorithm_name(std::string_view spec);
    algorithm_name(identifier plugin,
                   identifier algorithm,
                   specified_fields fields = specified_fields::both);

    std::string full() const;
    std::string const& plugin() const noexcept { return plugin_.trans_get_string(); }
    std::string const& algorithm() const noexcept { return algorithm_.trans_get_string(); }

    bool match(algorithm_name const& other) const;
    auto operator<=>(algorithm_name const&) const = default;

    static algorithm_name create(char const* spec);
    static algorithm_name create(std::string_view spec);

  private:
    auto cmp_tuple() const;
    identifier plugin_;
    identifier algorithm_;
    specified_fields fields_{specified_fields::neither};
  };

}

#endif // PHLEX_MODEL_ALGORITHM_NAME_HPP
