#include "phlex/core/consumer.hpp"

namespace phlex::experimental {
  consumer::consumer(algorithm_name name, std::vector<std::string> predicates) :
    name_{std::move(name)}, predicates_{std::move(predicates)}
  {
  }

  std::string consumer::full_name() const { return name_.full(); }

  identifier const& consumer::plugin() const noexcept { return name_.plugin(); }
  identifier const& consumer::algorithm() const noexcept { return name_.algorithm(); }

  std::vector<std::string> const& consumer::when() const noexcept { return predicates_; }
}
