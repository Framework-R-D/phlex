#include "phlex/model/product_specification.hpp"

#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/type_id.hpp"

#include <fmt/format.h>

#include <cassert>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace phlex::experimental {
  namespace {
    // Parses the `creator/suffix` form. The specification's type is left unspecified.
    product_specification parse(std::string_view s)
    {
      auto const forward_slash = s.find('/');
      if (forward_slash == std::string_view::npos) {
        throw std::runtime_error(
          fmt::format("The specification '{}' is not a valid product specification: "
                      "expected the form 'creator/suffix' (use 'creator/' for an empty suffix).",
                      s));
      }
      if (forward_slash == 0) {
        throw std::runtime_error(
          fmt::format("The product specification '{}' does not name a creator.", s));
      }
      return {algorithm_name{s.substr(0, forward_slash)},
              suffix_t(s.substr(forward_slash + 1)),
              type_id{}};
    }
  }

  product_specification::product_specification(char const* name) :
    product_specification{std::string_view{name}}
  {
  }
  product_specification::product_specification(std::string const& name) :
    product_specification{std::string_view{name}}
  {
  }
  product_specification::product_specification(std::string_view name) :
    product_specification{parse(name)}
  {
  }

  product_specification::product_specification(experimental::algorithm_name creator,
                                               experimental::suffix_t suffix,
                                               type_id type) :
    creator_{std::move(creator)}, suffix_{std::move(suffix)}, type_id_{std::move(type)}
  {
  }

  std::string product_specification::to_string() const
  {
    return fmt::format("{}/{}", creator_.to_string(), suffix_);
  }
}

namespace phlex::detail {
  experimental::product_specifications to_product_specifications(
    experimental::algorithm_name const& algo_name,
    std::vector<std::string> output_suffixes,
    type_ids output_types)
  {
    static std::string const default_product_suffix;

    if (output_suffixes.empty()) {
      // This happens whenever the user does not specify output_product_suffixes(...), so we
      // assign the default suffix to all output data products.
      output_suffixes.assign(output_types.size(), default_product_suffix);
    }

    // If output_suffixes and output_types are non-empty, the framework guarantees that they will
    // have the same length. They will either have the same length due to the default-suffix
    // assignment above, or because the call to output_product_suffixes(...) demands it.
    assert(output_suffixes.size() == output_types.size());

    // We can use std::views::zip_transform once the AppleClang C++ STL supports it.
    return std::views::zip(output_suffixes, output_types) |
           std::views::transform([&algo_name](auto const& p) {
             return experimental::product_specification{
               algo_name, experimental::suffix_t(std::get<0>(p)), std::get<1>(p)};
           }) |
           std::ranges::to<experimental::product_specifications>();
  }
}
