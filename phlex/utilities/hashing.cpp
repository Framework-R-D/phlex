#include "phlex/utilities/hashing.hpp"

#include "boost/functional/hash.hpp"
#include <boost/container_hash/hash.hpp>
#include <cstddef>
#include <functional>
#include <string>

namespace phlex::experimental {
  std::hash<std::string> const string_hasher{};

  auto hash(std::string const& str) -> std::size_t { return string_hasher(str); }

  auto hash(std::size_t i) noexcept -> std::size_t { return i; }

  auto hash(std::size_t i, std::size_t j) -> std::size_t
  {
    boost::hash_combine(i, j);
    return i;
  }

  auto hash(std::size_t i, std::string const& str) -> std::size_t { return hash(i, hash(str)); }
}
