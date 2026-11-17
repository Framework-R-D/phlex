#ifndef PHLEX_MODEL_DETAIL_EXCERPT_HPP
#define PHLEX_MODEL_DETAIL_EXCERPT_HPP

// Private to the phlex_model library; this header is intentionally not installed.

#include <fmt/format.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace phlex::detail {
  /// The longest amount of user-supplied text quoted in an error message.
  inline constexpr std::size_t max_excerpt_length{64};

  /// Quotes and escapes `text` for an error message, shortening it with a trailing `...`
  /// inside the quotes when it exceeds `max_excerpt_length` characters.
  inline std::string excerpt(std::string_view text)
  {
    std::string shown{text.substr(0, max_excerpt_length)};
    if (text.size() > max_excerpt_length) {
      shown += "...";
    }
    return fmt::format("{:?}", shown);
  }
}

#endif // PHLEX_MODEL_DETAIL_EXCERPT_HPP
