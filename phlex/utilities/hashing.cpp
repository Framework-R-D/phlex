#include "phlex/utilities/hashing.hpp"

#include <boost/container_hash/hash.hpp>
#include <boost/functional/hash.hpp>

#include <cstddef>
#include <functional>
#include <string>

namespace phlex::detail {
  std::hash<std::string> const string_hasher{};

  std::size_t hash(std::string const& str) { return string_hasher(str); }

  std::size_t hash(std::size_t i) noexcept { return i; }

  std::size_t hash(std::size_t i, std::size_t j)
  {
    boost::hash_combine(i, j);
    return i;
  }

  std::size_t hash(std::size_t i, std::string const& str) { return hash(i, hash(str)); }
}
