#include "identifier.hpp"

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/xxhash.hpp>

namespace phlex::experimental {
  identifier_query literals::operator""_idq(char const* lit, std::size_t len)
  {
    return {identifier::hash_string(std::string_view(lit, len))};
  }

  std::uint64_t identifier::hash_string(std::string_view str)
  {
    // Hash quality is very important here, since comparisons are done using only the hash
    using namespace boost::hash2;
    xxhash_64 h;
    hash_append(h, {}, str);
    return h.result();
  }

  identifier::identifier(std::string_view str) : content_(str), hash_(hash_string(content_)) {}
  identifier& identifier::operator=(std::string_view const& str)
  {
    content_ = str;
    hash_ = hash_string(content_);
    return *this;
  }

  identifier::operator std::string_view() const noexcept { return std::string_view(content_); }
  bool identifier::operator==(identifier const& rhs) const noexcept { return hash_ == rhs.hash_; }
  std::strong_ordering identifier::operator<=>(identifier const& rhs) const noexcept
  {
    return hash_ <=> rhs.hash_;
  }

  bool operator==(identifier const& lhs, identifier_query rhs) { return lhs.hash_ == rhs.hash; }
  std::strong_ordering operator<=>(identifier const& lhs, identifier_query rhs)
  {
    return lhs.hash_ <=> rhs.hash;
  }

  identifier literals::operator""_id(char const* lit, std::size_t len)
  {
    return identifier{std::string_view(lit, len)};
  }
}
