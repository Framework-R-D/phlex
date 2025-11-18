#include "phlex/model/qualified_name.hpp"
#include "phlex/model/algorithm_name.hpp"

#include <cassert>

namespace phlex::experimental {
  qualified_name::qualified_name() = default;

  qualified_name::qualified_name(char const* name) : qualified_name{std::string{name}} {}
  qualified_name::qualified_name(std::string name) { *this = create(name); }

  qualified_name::qualified_name(algorithm_name qualifier, std::string name, type_id type) :
    qualifier_{std::move(qualifier)}, name_{std::move(name)}, type_id_{type}
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
    return std::tie(qualifier_, name_, type_id_) ==
           std::tie(other.qualifier_, other.name_, other.type_id_);
  }

  bool qualified_name::operator!=(qualified_name const& other) const { return !operator==(other); }

  bool qualified_name::operator<(qualified_name const& other) const
  {
    return std::tie(qualifier_, name_, type_id_) <
           std::tie(other.qualifier_, other.name_, type_id_);
  }

  qualified_name qualified_name::create(char const* c) { return create(std::string{c}); }

  qualified_name qualified_name::create(std::string const& s)
  {
    auto forward_slash = s.find("/");
    if (forward_slash != std::string::npos) {
      return {
        algorithm_name::create(s.substr(0, forward_slash)), s.substr(forward_slash + 1), type_id{}};
    }
    return {algorithm_name::create(""), s, type_id{}};
  }

  qualified_names to_qualified_names(std::string const& name,
                                     std::vector<std::string> output_labels,
                                     std::vector<type_id> output_types)
  {
    assert(output_labels.size() == output_types.size());
    qualified_names outputs;
    outputs.reserve(output_labels.size());

    to_qualified_name make_qualified_name{name};
    // zip view isn't available until C++23 so we have to use a loop over the index
    for (std::size_t i = 0; i < output_labels.size(); ++i) {
      outputs.push_back(make_qualified_name(output_labels.at(i), output_types.at(i)));
    }
    return outputs;
  }
}
