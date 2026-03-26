#include "phlex/model/product_specification.hpp"
#include "phlex/model/algorithm_name.hpp"

#include <cassert>
#include <ranges>
#include <stdexcept>
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
                                               identifier suffix,
                                               type_id type) :
    qualifier_{std::move(qualifier)}, suffix_{std::move(suffix)}, type_id_{std::move(type)}
  {
  }

  std::string product_specification::full() const
  {
    if (qualifier_.plugin().empty() && qualifier_.algorithm().empty()) {
      return fmt::format("{}", suffix_);
    }
    return fmt::format("{}/{}", qualifier_.full(), suffix_);
  }

  product_specification product_specification::create(char const* c)
  {
    return create(std::string_view{c});
  }

  product_specification product_specification::create(std::string_view s)
  {
    auto forward_slash = s.find('/');
    if (forward_slash != std::string_view::npos) {
      return {algorithm_name::create(s.substr(0, forward_slash)),
              identifier(s.substr(forward_slash + 1)),
              type_id{}};
    }
    return {algorithm_name::create(""), identifier(s), type_id{}};
  }

  product_specifications to_product_specifications(std::string_view const algorithm_specification,
                                                   std::vector<std::string> output_suffixes,
                                                   std::vector<type_id> output_types)
  {
    if (output_suffixes.empty()) {
      output_suffixes.assign(output_types.size(), default_suffix);
    } else if (output_suffixes.size() != output_types.size()) {
      throw std::runtime_error{fmt::format(
        "Number of output product suffixes ({}) does not match number of output product types ({})",
        output_suffixes.size(),
        output_types.size())};
    }

    // We can use std::views::zip_transform once the AppleClang C++ STL supports it.
    auto const algo_name = algorithm_name::create(algorithm_specification);
    return std::views::zip(output_suffixes, output_types) |
           std::views::transform([&algo_name](auto const& p) {
             return product_specification{algo_name, identifier(std::get<0>(p)), std::get<1>(p)};
           }) |
           std::ranges::to<product_specifications>();
  }
}
