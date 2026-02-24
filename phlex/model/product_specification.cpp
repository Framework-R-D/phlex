#include "phlex/model/product_specification.hpp"
#include "phlex/model/algorithm_name.hpp"

#include <cassert>
#include <utility>

namespace phlex::experimental {
  product_specification::product_specification() = default;

  product_specification::product_specification(char const* name) :
    product_specification{std::string_view{name}}
  {
  }
  product_specification::product_specification(std::string const& name) :
    product_specification{std::string_view{name}}
  {
  }
  product_specification::product_specification(std::string_view name) { *this = create(name); }

  product_specification::product_specification(algorithm_name qualifier,
                                               identifier name,
                                               type_id type) :
    qualifier_{std::move(qualifier)}, name_{std::move(name)}, type_id_{std::move(type)}
  {
  }

  std::string product_specification::full() const
  {
    if (qualifier_.plugin().empty() && qualifier_.algorithm().empty()) {
      return fmt::format("{}", name_);
    }
    return fmt::format("{}/{}", qualifier_.full(), name_);
  }

  product_specification product_specification::create(char const* c)
  {
    return create(std::string_view{c});
  }

  product_specification product_specification::create(std::string_view s)
  {
    auto forward_slash = s.find('/');
    if (forward_slash != std::string_view::npos) {
      return {
        algorithm_name::create(s.substr(0, forward_slash)), identifier(s.substr(forward_slash + 1)), type_id{}};
    }
    return {algorithm_name::create(""), identifier(s), type_id{}};
  }

  product_specifications to_product_specifications(std::string_view const name,
                                                   std::vector<std::string> output_labels,
                                                   std::vector<type_id> output_types)
  {
    assert(output_labels.size() == output_types.size());
    product_specifications outputs;
    outputs.reserve(output_labels.size());

    // zip view isn't available until C++23 so we have to use a loop over the index
    for (std::size_t i = 0; i < output_labels.size(); ++i) {
      outputs.emplace_back(algorithm_name::create(name), identifier(output_labels[i]), output_types[i]);
    }
    return outputs;
  }
}
