#ifndef PHLEX_UTILITIES_SIGNED_SIZE_HPP
#define PHLEX_UTILITIES_SIGNED_SIZE_HPP

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace phlex::detail {
  using signed_size_t = std::make_signed_t<std::size_t>;

  inline signed_size_t checked_signed_size(std::size_t const value)
  {
    if (std::cmp_greater(value, std::numeric_limits<signed_size_t>::max())) {
      throw std::overflow_error{"Value exceeds signed size range"};
    }
    return static_cast<signed_size_t>(value);
  }
}

#endif // PHLEX_UTILITIES_SIGNED_SIZE_HPP
