#include "phlex/utilities/hashing.hpp"

#include <boost/container_hash/hash.hpp>
#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/xxhash.hpp>

namespace phlex::experimental {
  std::size_t hash(std::string const& str)
  {
    using namespace boost::hash2;
    xxhash_64 h;
    hash_append(h, {}, str);
    return h.result();
  }

  std::size_t hash(std::size_t i) noexcept { return i; }

  std::size_t hash(std::size_t i, std::size_t j)
  {
    boost::hash_combine(i, j);
    return i;
  }

  std::size_t hash(std::size_t i, std::string const& str)
  {
    using namespace boost::hash2;
    xxhash_64 h{i};
    hash_append(h, {}, str);
    return h.result();
  }
}
