#include "phlex/model/product_specification.hpp"

#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/type_id.hpp"

#include <fmt/format.h>

#include <cassert>
#include <optional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

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

  product_specification::product_specification(experimental::algorithm_name creator,
                                               experimental::suffix_t suffix,
                                               type_id type) :
    creator_{std::move(creator)}, suffix_{std::move(suffix)}, type_id_{std::move(type)}
  {
  }

  product_specification::product_specification(std::nullopt_t, suffix_t suffix, type_id type) :
    suffix_{std::move(suffix)}, type_id_{std::move(type)}
  {
  }

  std::string product_specification::to_string() const
  {
    if (!creator_) {
      return fmt::format("{}", suffix_);
    }
    return fmt::format("{}/{}", creator_->to_string(), suffix_);
  }

  product_specification product_specification::create(char const* c)
  {
    return create(std::string_view{c});
  }

  product_specification product_specification::create(std::string_view s)
  {
    auto forward_slash = s.find('/');
    if (forward_slash != std::string_view::npos) {
      if (forward_slash == 0) {
        return {std::nullopt, suffix_t(s.substr(1)), type_id{}};
      }
      return {experimental::algorithm_name::create(s.substr(0, forward_slash)),
              experimental::suffix_t(s.substr(forward_slash + 1)),
              type_id{}};
    }
    return {std::nullopt, experimental::suffix_t(s), type_id{}};
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
