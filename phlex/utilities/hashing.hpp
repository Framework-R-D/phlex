#ifndef PHLEX_UTILITIES_HASHING_HPP
#define PHLEX_UTILITIES_HASHING_HPP

#include "phlex/phlex_utilities_export.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace phlex::experimental {
  PHLEX_UTILITIES_EXPORT std::size_t hash(std::string const& str);
  PHLEX_UTILITIES_EXPORT std::size_t hash(std::size_t i) noexcept;
  PHLEX_UTILITIES_EXPORT std::size_t hash(std::size_t i, std::size_t j);
  PHLEX_UTILITIES_EXPORT std::size_t hash(std::size_t i, std::string const& str);
  template <typename... Ts>
  std::size_t hash(std::size_t i, std::size_t j, Ts... ks)
  {
    return hash(hash(i, j), ks...);
  }
}

#endif // PHLEX_UTILITIES_HASHING_HPP
