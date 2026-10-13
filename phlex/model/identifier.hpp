#ifndef PHLEX_MODEL_IDENTIFIER_HPP
#define PHLEX_MODEL_IDENTIFIER_HPP

#include "phlex/phlex_model_export.hpp"

#include <boost/hash2/hash_append.hpp>
#include <boost/hash2/xxhash.hpp>
#include <boost/json/fwd.hpp>
#include <fmt/format.h>

#include <compare>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace phlex::experimental {
  /// If you're comparing to an identifier you know at compile time, you're probably not going to need
  /// to print it.
  class identifier;
  struct PHLEX_MODEL_EXPORT identifier_query {
    std::uint64_t hash;
    // This means an identifier_query can be used as a callable to check if it compares equal to an identifier
    bool operator()(identifier const& id) const noexcept;
  };

  /// Carries around the string itself (as a shared_ptr to string to make copies lighter)
  /// along with a precomputed hash used for all comparisons
  class PHLEX_MODEL_EXPORT identifier {
  public:
    static constexpr std::uint64_t hash_string(std::string_view str)
    {
      // Hash quality is very important here, since comparisons are done using only the hash
      using namespace boost::hash2;
      xxhash_64 h;
      hash_append(h, {}, str);
      return h.result();
    }
    // The default constructor is necessary so other classes containing identifiers
    // can have default constructors.
    identifier() = default;
    identifier(identifier const& other) = default;
    identifier(identifier&& other) noexcept = default;

    explicit identifier(std::string_view str);

    // This is here to allow the node API which heretofore stored names as strings to be easily transitioned
    // over to identifiers
    explicit identifier(std::string&& str);

    // char const* calls string_view
    // NOLINTNEXTLINE(google-explicit-constructor) - Implicit conversion is intentional
    identifier(char const* lit) : identifier(std::string_view(lit)) {}

    identifier& operator=(identifier const& rhs) = default;
    identifier& operator=(identifier&& rhs) noexcept = default;

    ~identifier() = default;

    // Conversion to std::string_view
    explicit operator std::string_view() const noexcept;

    bool operator==(identifier const& rhs) const noexcept;
    std::strong_ordering operator<=>(identifier const& rhs) const noexcept;

    // check if empty
    bool empty() const noexcept { return content_.empty(); }
    // get hash
    std::size_t hash() const noexcept { return hash_; }

    // transitional access to contained string
    std::string const& trans_get_string() const noexcept { return content_; }

    // Comparison operators with _id queries
    friend PHLEX_MODEL_EXPORT bool operator==(identifier const& lhs, identifier_query rhs);
    friend PHLEX_MODEL_EXPORT std::strong_ordering operator<=>(identifier const& lhs,
                                                               identifier_query rhs);
    friend std::hash<identifier>;

  private:
    std::string content_;
    std::uint64_t hash_{hash_string("")};
  };

  // Identifier UDL
  namespace literals {
    PHLEX_MODEL_EXPORT identifier operator""_id(char const* lit, std::size_t len);
    consteval PHLEX_MODEL_EXPORT identifier_query operator""_idq(char const* lit, std::size_t len)
    {
      return {identifier::hash_string(std::string_view(lit, len))};
    }

  }

  // Really trying to avoid the extra function call here
  inline std::string_view format_as(identifier const& id) { return std::string_view(id); }
  inline std::size_t hash_value(identifier const& id) { return id.hash(); }

  /// The explicit representation of a meaningful empty textual value (for example, an empty
  /// product suffix). It is distinct from an omitted value (`std::nullopt` in a selector) and
  /// from any named `identifier`.
  struct empty_string {
    constexpr bool operator==(empty_string const&) const noexcept = default;
    constexpr std::strong_ordering operator<=>(empty_string const&) const noexcept = default;
  };

  /// A value that is either `empty_string` or a named `identifier`.
  ///
  /// Value states:
  /// - empty: holds `empty_string`; `text()` and formatting produce `""`.
  /// - named: holds an `identifier`; `text()` and formatting produce the identifier's text.
  ///
  /// Value semantics:
  /// - Equality holds only between two empty values or two named values with equal text.
  ///   An empty value never equals a named value, even one whose text is empty.
  /// - Ordering places every empty value before every named value; named values are ordered by
  ///   `identifier`'s ordering.
  /// - Hashing gives all empty values the same hash, and named values the hash of their
  ///   identifier.
  ///
  /// The default constructor produces the empty state.
  class PHLEX_MODEL_EXPORT identifier_or_empty {
  public:
    identifier_or_empty() noexcept = default;

    // NOLINTNEXTLINE(google-explicit-constructor) - Implicit conversion is intentional
    identifier_or_empty(empty_string) noexcept : value_{empty_string{}} {}

    // NOLINTNEXTLINE(google-explicit-constructor) - Implicit conversion is intentional
    identifier_or_empty(identifier const& id) : value_{id} {}

    // NOLINTNEXTLINE(google-explicit-constructor) - Implicit conversion is intentional
    identifier_or_empty(identifier&& id) noexcept : value_{std::move(id)} {}

    /// True when this value holds `empty_string`.
    bool empty() const noexcept { return std::holds_alternative<empty_string>(value_); }

    /// Textual view of the value: `""` for the empty state, the identifier's text otherwise.
    std::string_view text() const noexcept
    {
      if (auto const* id = std::get_if<identifier>(&value_)) {
        return std::string_view(*id);
      }
      return {};
    }

    /// The named identifier, or nullptr when this value is empty.
    identifier const* named() const noexcept { return std::get_if<identifier>(&value_); }

    /// Hash of the value; all empty values share the hash `0`.
    std::size_t hash() const noexcept
    {
      if (auto const* id = std::get_if<identifier>(&value_)) {
        return id->hash();
      }
      return 0;
    }

    bool operator==(identifier_or_empty const& rhs) const noexcept { return value_ == rhs.value_; }

    std::strong_ordering operator<=>(identifier_or_empty const& rhs) const noexcept
    {
      if (value_.index() != rhs.value_.index()) {
        return value_.index() <=> rhs.value_.index();
      }
      if (auto const* id = std::get_if<identifier>(&value_)) {
        return *id <=> std::get<identifier>(rhs.value_);
      }
      return std::strong_ordering::equal;
    }

  private:
    // Index 0 is the empty state, so empty values order before named values.
    std::variant<empty_string, identifier> value_{empty_string{}};
  };

  inline std::string_view format_as(empty_string) { return {}; }
  inline std::string_view format_as(identifier_or_empty const& value) { return value.text(); }
  inline std::size_t hash_value(identifier_or_empty const& value) { return value.hash(); }
}

template <>
struct std::hash<phlex::experimental::identifier> {
  std::size_t operator()(phlex::experimental::identifier const& id) const noexcept
  {
    return id.hash_;
  }
};

template <>
struct std::hash<phlex::experimental::identifier_or_empty> {
  std::size_t operator()(phlex::experimental::identifier_or_empty const& value) const noexcept
  {
    return value.hash();
  }
};

#endif // PHLEX_MODEL_IDENTIFIER_HPP
