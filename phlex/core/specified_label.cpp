#include "phlex/core/specified_label.hpp"


#include <cstddef>
#include <fmt/core.h>
#include <ostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

namespace phlex::experimental {
  auto specified_label::operator()(std::string family) && -> specified_label
  {
    return {.name=std::move(name), .family=std::move(family)};
  }

  auto specified_label::to_string() const -> std::string
  {
    if (family.empty()) {
      return name.full();
    }
    return fmt::format("{} ϵ {}", name.full(), family);
  }

  auto operator""_in(char const* name, std::size_t length) -> specified_label
  {
    if (length == 0ull) {
      throw std::runtime_error("Cannot specify product with empty name.");
    }
    return specified_label::create(name);
  }

  auto operator==(specified_label const& a, specified_label const& b) -> bool
  {
    return std::tie(a.name, a.family) == std::tie(b.name, b.family);
  }

  auto operator!=(specified_label const& a, specified_label const& b) -> bool { return !(a == b); }

  auto operator<(specified_label const& a, specified_label const& b) -> bool
  {
    return std::tie(a.name, a.family) < std::tie(b.name, b.family);
  }

  auto operator<<(std::ostream& os, specified_label const& label) -> std::ostream&
  {
    os << label.to_string();
    return os;
  }

  auto specified_label::create(char const* c) -> specified_label { return create(std::string{c}); }

  auto specified_label::create(std::string const& s) -> specified_label
  {
    return {.name=qualified_name::create(s)};
  }

  auto specified_label::create(specified_label l) -> specified_label { return l; }
}
