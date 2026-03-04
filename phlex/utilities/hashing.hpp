#ifndef PHLEX_UTILITIES_HASHING_HPP
#define PHLEX_UTILITIES_HASHING_HPP

#include "phlex_utilities_export.hpp"

#include <cstdint>
#include <string>

namespace phlex::experimental {
  phlex_utilities_EXPORT std::size_t hash(std::string const& str);
  phlex_utilities_EXPORT std::size_t hash(std::size_t i) noexcept;
  phlex_utilities_EXPORT std::size_t hash(std::size_t i, std::size_t j);
  phlex_utilities_EXPORT std::size_t hash(std::size_t i, std::string const& str);
  template <typename... Ts>
  std::size_t hash(std::size_t i, std::size_t j, Ts... ks)
  {
    return hash(hash(i, j), ks...);
  }
}

#endif // PHLEX_UTILITIES_HASHING_HPP
