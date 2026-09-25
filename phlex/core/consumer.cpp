#include "phlex/core/consumer.hpp"

#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"

#include <string>
#include <utility>
#include <vector>

namespace phlex::detail {
  consumer::consumer(phlex::experimental::algorithm_name name,
                     std::vector<std::string> predicates) :
    name_{std::move(name)}, predicates_{std::move(predicates)}
  {
  }

  phlex::experimental::algorithm_name const& consumer::name() const noexcept { return name_; }
  phlex::experimental::identifier const& consumer::plugin() const noexcept
  {
    return name_.plugin();
  }
  phlex::experimental::identifier const& consumer::algorithm() const noexcept
  {
    return name_.algorithm();
  }

  std::vector<std::string> const& consumer::when() const noexcept { return predicates_; }
}
