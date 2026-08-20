#ifndef PHLEX_CORE_CONSUMER_HPP
#define PHLEX_CORE_CONSUMER_HPP

#include "phlex/phlex_core_export.hpp"

#include "phlex/model/algorithm_name.hpp"

#include <string>
#include <vector>

namespace phlex::detail {
  class PHLEX_CORE_EXPORT consumer {
  public:
    consumer(phlex::experimental::algorithm_name name, std::vector<std::string> predicates);

    phlex::experimental::algorithm_name const& name() const noexcept;
    phlex::experimental::identifier const& plugin() const noexcept;
    phlex::experimental::identifier const& algorithm() const noexcept;
    std::vector<std::string> const& when() const noexcept;

  private:
    phlex::experimental::algorithm_name name_;
    std::vector<std::string> predicates_;
  };
}

#endif // PHLEX_CORE_CONSUMER_HPP
