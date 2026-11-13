#include "phlex/model/identifier.hpp"

#include <compare>
#include <cstddef>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace phlex::experimental {
  namespace {
    void validate_identifier(std::string_view str)
    {
      if (!phlex::detail::valid_identifier(str)) {
        throw std::invalid_argument{"Invalid identifier: expected an ASCII letter followed by "
                                    "ASCII letters, digits, or underscores"};
      }
    }
  }

  identifier::identifier(std::string_view str) : content_(str), hash_(hash_string(content_))
  {
    validate_identifier(content_);
  }
  identifier::identifier(std::string&& str) : content_(std::move(str)), hash_(hash_string(content_))
  {
    validate_identifier(content_);
  }

  identifier::operator std::string_view() const noexcept { return std::string_view(content_); }
  bool identifier::operator==(identifier const& rhs) const noexcept
  {
    if (hash_ == rhs.hash_) {
      return content_ == rhs.content_;
    }
    return false;
  }
  std::strong_ordering identifier::operator<=>(identifier const& rhs) const noexcept
  {
    std::strong_ordering hash_cmp = hash_ <=> rhs.hash_;
    if (hash_cmp == 0) {
      return content_ <=> rhs.content_;
    }
    return hash_cmp;
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

  bool identifier_query::operator()(identifier const& id) const noexcept { return id == (*this); }
}
