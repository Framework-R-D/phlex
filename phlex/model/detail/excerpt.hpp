#ifndef PHLEX_MODEL_DETAIL_EXCERPT_HPP
#define PHLEX_MODEL_DETAIL_EXCERPT_HPP

// Private to the phlex_model library; this header is intentionally not installed.

#include <cstddef>
#include <string>
#include <string_view>

namespace phlex::detail {
  /// The longest amount of user-supplied text quoted in an error message.
  inline constexpr std::size_t max_excerpt_length{64};

  /// Quotes and escapes `text` for an error message, shortening it with a trailing `...`
  /// inside the quotes when it exceeds `max_excerpt_length` characters.
  std::string excerpt(std::string_view text);
}

#endif // PHLEX_MODEL_DETAIL_EXCERPT_HPP
