#include "phlex/model/product_specification.hpp"
#include "phlex/model/algorithm_name.hpp"

#include <cassert>

namespace phlex::experimental {
  product_specification::product_specification() = default;

  product_specification::product_specification(char const* name) : product_specification{std::string{name}} {}
  product_specification::product_specification(std::string name) { *this = create(name); }

  product_specification::product_specification(algorithm_name qualifier, std::string name, type_id type) :
    qualifier_{std::move(qualifier)}, name_{std::move(name)}, type_id_{type}
  {
  }

  std::string product_specification::full() const
  {
    auto const qualifier = qualifier_.full();
    if (qualifier.empty()) {
      return name_;
    }
    return qualifier + "/" + name_;
  }

  bool product_specification::operator==(product_specification const& other) const
  {
    return std::tie(qualifier_, name_, type_id_) ==
           std::tie(other.qualifier_, other.name_, other.type_id_);
  }

  bool product_specification::operator!=(product_specification const& other) const { return !operator==(other); }

  bool product_specification::operator<(product_specification const& other) const
  {
    return std::tie(qualifier_, name_, type_id_) <
           std::tie(other.qualifier_, other.name_, type_id_);
  }

  product_specification product_specification::create(char const* c) { return create(std::string{c}); }

  product_specification product_specification::create(std::string const& s)
  {
    auto forward_slash = s.find("/");
    if (forward_slash != std::string::npos) {
      return {
        algorithm_name::create(s.substr(0, forward_slash)), s.substr(forward_slash + 1), type_id{}};
    }
    return {algorithm_name::create(""), s, type_id{}};
  }

  product_specifications to_product_specifications(std::string const& name,
                                     std::vector<std::string> output_labels,
                                     std::vector<type_id> output_types)
  {
    assert(output_labels.size() == output_types.size());
    product_specifications outputs;
    outputs.reserve(output_labels.size());

    to_product_specification make_product_specification{name};
    // zip view isn't available until C++23 so we have to use a loop over the index
    for (std::size_t i = 0; i < output_labels.size(); ++i) {
      outputs.push_back(make_product_specification(output_labels.at(i), output_types.at(i)));
    }
    return outputs;
  }
}
