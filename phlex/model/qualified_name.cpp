#include "phlex/model/qualified_name.hpp"
#include "phlex/model/algorithm_name.hpp"

#include <algorithm>
#include <iterator>
#include <tuple>
#include <utility>
#include <vector>

namespace phlex::experimental {
  qualified_name::qualified_name() = default;

  qualified_name::qualified_name(char const* name) : qualified_name{std::string{name}} {}
  qualified_name::qualified_name(std::string const& name) { *this = create(name); }

  qualified_name::qualified_name(algorithm_name qualifier, std::string name) :
    qualifier_{std::move(qualifier)}, name_{std::move(name)}
  {
  }

  std::string qualified_name::full() const
  {
    auto const qualifier = qualifier_.full();
    if (qualifier.empty()) {
      return name_;
    }
    return qualifier + "/" + name_;
  }

  bool qualified_name::operator==(qualified_name const& other) const
  {
    return std::tie(qualifier_, name_) == std::tie(other.qualifier_, other.name_);
  }

  bool qualified_name::operator!=(qualified_name const& other) const { return !operator==(other); }

  bool qualified_name::operator<(qualified_name const& other) const
  {
    return std::tie(qualifier_, name_) < std::tie(other.qualifier_, other.name_);
  }

  qualified_name qualified_name::create(char const* c) { return create(std::string{c}); }

  qualified_name qualified_name::create(std::string const& s)
  {
    auto forward_slash = s.find('/');
    if (forward_slash != std::string::npos) {
      return {algorithm_name::create(s.substr(0, forward_slash)), s.substr(forward_slash + 1)};
    }
    return {algorithm_name::create(""), s};
  }

  qualified_names to_qualified_names(std::string const& name,
                                     std::vector<std::string> output_labels)
  {
    qualified_names outputs;
    outputs.reserve(output_labels.size());
    std::ranges::transform(output_labels, std::back_inserter(outputs), to_qualified_name{name});
    return outputs;
  }
}
