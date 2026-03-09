#ifndef PHLEX_CORE_CONSUMER_HPP
#define PHLEX_CORE_CONSUMER_HPP

#include "phlex_core_export.hpp"

#include "phlex/model/algorithm_name.hpp"

#include <string>
#include <vector>

namespace phlex::experimental {
  class phlex_core_EXPORT consumer {
  public:
    consumer(algorithm_name name, std::vector<std::string> predicates);

    std::string full_name() const;
    identifier const& plugin() const noexcept;
    identifier const& algorithm() const noexcept;
    std::vector<std::string> const& when() const noexcept;

  private:
    algorithm_name name_;
    std::vector<std::string> predicates_;
  };
}

#endif // PHLEX_CORE_CONSUMER_HPP
