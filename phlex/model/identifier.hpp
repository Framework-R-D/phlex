#ifndef PHLEX_MODEL_IDENTIFIER_H_
#define PHLEX_MODEL_IDENTIFIER_H_

#include <boost/json/fwd.hpp>

#include <fmt/format.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace phlex::experimental {
  /// If you're comparing to an identifier you know at compile time, you're probably not going to need
  /// to print it.
  struct identifier_query {
    std::uint64_t hash;
  };

  /// Carries around the string itself (as a shared_ptr to string to make copies lighter)
  /// along with a precomputed hash used for all comparisons
  class identifier {
  public:
    static std::uint64_t hash_string(std::string_view const& str);
    identifier(identifier const& other);
    identifier(identifier&& other) noexcept;

    explicit identifier(std::string_view const& str);

    identifier& operator=(identifier const& rhs);
    identifier& operator=(identifier&& rhs) noexcept;

    // Assignment for identifiers read from a file
    identifier& operator=(std::string_view const& str);

    ~identifier();

    // Conversion to std::string_view
    explicit operator std::string_view() const noexcept;

    bool operator==(identifier const& rhs) const noexcept;
    std::strong_ordering operator<=>(identifier const& rhs) const noexcept;

    // Comparison operators with _id queries
    friend bool operator==(identifier const& lhs, identifier_query rhs);
    friend std::strong_ordering operator<=>(identifier const& lhs, identifier_query rhs);
    friend std::hash<identifier>;

  private:
    std::shared_ptr<std::string const> content_;
    std::uint64_t hash_;
  };

  // Identifier UDL
  namespace literals {
    identifier operator""_id(char const* lit, std::size_t len);
    identifier_query operator""_idq(char const* lit, std::size_t len);
  }

  // Really trying to avoid the extra function call here
  inline std::string_view format_as(identifier const& id) { return std::string_view(id); }
}

template <>
struct std::hash<phlex::experimental::identifier> {
  std::size_t operator()(phlex::experimental::identifier const& id) const noexcept
  {
    return id.hash_;
  }
};

#endif // PHLEX_MODEL_IDENTIFIER_H_
