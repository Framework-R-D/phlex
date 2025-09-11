#include "phlex/core/specified_label.hpp"

#include "fmt/format.h"

#include <ostream>
#include <stdexcept>
#include <tuple>

namespace phlex::experimental {
  specified_label specified_label::operator()(std::string family) &&
  {
    return {std::move(name), std::move(family)};
  }

  std::string specified_label::to_string() const
  {
    if (family.empty()) {
      return name.full();
    }
    return fmt::format("{} ϵ {}", name.full(), family);
  }

  specified_label operator""_in(char const* name, std::size_t length)
  {
    if (length == 0ull) {
      throw std::runtime_error("Cannot specify product with empty name.");
    }
    return specified_label::create(name);
  }

  bool operator==(specified_label const& a, specified_label const& b)
  {
    return std::tie(a.name, a.family) == std::tie(b.name, b.family);
  }

  bool operator!=(specified_label const& a, specified_label const& b) { return !(a == b); }

  bool operator<(specified_label const& a, specified_label const& b)
  {
    return std::tie(a.name, a.family) < std::tie(b.name, b.family);
  }

  std::ostream& operator<<(std::ostream& os, specified_label const& label)
  {
    os << label.to_string();
    return os;
  }

  specified_label specified_label::create(char const* c) { return create(std::string{c}); }

  specified_label specified_label::create(std::string const& s)
  {
    return {qualified_name::create(s)};
  }

  specified_label specified_label::create(specified_label l) { return l; }
}
