#include "phlex/core/consumer.hpp"
#include "phlex/model/algorithm_name.hpp"
#include <string>
#include <utility>
#include <vector>

namespace phlex::experimental {
  consumer::consumer(algorithm_name name, std::vector<std::string> predicates) :
    name_{std::move(name)}, predicates_{std::move(predicates)}
  {
  }

  auto consumer::full_name() const -> std::string { return name_.full(); }

  auto consumer::plugin() const noexcept -> std::string const& { return name_.plugin(); }
  auto consumer::algorithm() const noexcept -> std::string const& { return name_.algorithm(); }

  auto consumer::when() const noexcept -> std::vector<std::string> const& { return predicates_; }
}
