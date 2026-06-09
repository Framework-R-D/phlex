#ifndef PHLEX_UTILITIES_BULLETED_LIST_HPP
#define PHLEX_UTILITIES_BULLETED_LIST_HPP

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <ranges>
#include <string>

namespace phlex::experimental {
  namespace detail {
    template <typename R>
    concept range_of_formattable = requires(std::ranges::range_value_t<R> const& val) {
      { fmt::format("{}", val) } -> std::same_as<std::string>;
    };

    template <typename R>
    concept range_of_to_stringable = requires(std::ranges::range_value_t<R> const& val) {
      !range_of_formattable<R>;
      { val.to_string() } -> std::same_as<std::string>;
    };
  }

  template <detail::range_of_formattable R>
  std::string bulleted_list(R const& rng, std::size_t indent = 2)
  {
    std::string prefix =
      fmt::format("{blank:{indent}s}- ", fmt::arg("blank", ""), fmt::arg("indent", indent));
    std::string prefix_with_newline = fmt::format("\n{}", prefix);
    return fmt::format("{}{}", prefix, fmt::join(rng, prefix_with_newline));
  }

  template <detail::range_of_to_stringable R>
  std::string bulleted_list(R const& rng, std::size_t indent = 2)
  {
    std::string prefix =
      fmt::format("{blank:{indent}s}- ", fmt::arg("blank", ""), fmt::arg("indent", indent));
    std::string prefix_with_newline = fmt::format("\n{}", prefix);
    return fmt::format(
      "{}{}",
      prefix,
      fmt::join(rng | std::views::transform([](auto&& v) { return v.to_string(); }),
                prefix_with_newline));
  }
}

#endif // PHLEX_UTILITIES_BULLETED_LIST_HPP
