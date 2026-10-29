#ifndef PHLEX_MODEL_PRODUCT_SPECIFICATION_HPP
#define PHLEX_MODEL_PRODUCT_SPECIFICATION_HPP

#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/type_id.hpp"
#include "phlex/phlex_model_export.hpp"

#include <boost/container_hash/hash.hpp>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace phlex::experimental {
  /// Describes a product supplied by an algorithm.
  ///
  /// A product specification is a partial label: it carries the creator, suffix, and type of a
  /// product. Its metadata combines with a concrete data-cell/provider layer to form the complete
  /// label of a created product, so the layer is deliberately not a member of this class.
  ///
  /// The creator is optional. A specification without a creator is valid for exact storage and
  /// lookup (for example, the suffix-only text `hits`, `/hits`, `""`, or `/`); it is not a
  /// wildcard. Equality, ordering, and hashing distinguish an absent creator from every present
  /// creator: an absent creator orders before any present creator and equals only another absent
  /// creator. Provider construction requires a present creator.
  ///
  /// The suffix is a `suffix_t`: either the explicit empty value (`empty_string`) or a named
  /// `identifier`. Parsing, defaulting, formatting, equality, ordering, and hashing all preserve
  /// that representation without manufacturing an empty `identifier`. The default suffix is
  /// explicitly empty. It is never an omitted selection criterion, which is `std::nullopt` in a
  /// product selector.
  ///
  /// Textual boundary: suffixes supplied as text (the `creator/suffix` text accepted by
  /// `create()`, and the strings given to the fluent `output_product_suffixes(...)` registration
  /// API, which `phlex::detail::to_product_specifications()` converts) remain strings. Empty text
  /// becomes `empty_string` and non-empty text becomes a named `identifier`, when the
  /// specification is created. Non-empty text that is not a valid name is rejected rather than
  /// stored as an empty suffix.
  ///
  /// This source-provider construction API is experimental and may change before it is
  /// promoted to the stable phlex namespace.
  class PHLEX_MODEL_EXPORT product_specification {
  public:
    /// Constructs a specification with an absent creator and an empty suffix.
    product_specification();
    // NOLINTBEGIN(google-explicit-constructor) - Implicit conversion is intentional
    product_specification(char const* name);
    product_specification(std::string const& name);
    product_specification(std::string_view name);
    // NOLINTEND(google-explicit-constructor)
    /// An absent creator is spelled `std::nullopt`; any `algorithm_name` supplies a present one.
    product_specification(std::optional<algorithm_name> creator, suffix_t suffix, type_id type);

    /// Text of the specification: `creator/suffix` for a present creator, and only the suffix
    /// text for an absent creator.
    std::string to_string() const;

    /// The creator, or `std::nullopt` if it is absent.
    std::optional<algorithm_name> const& creator() const noexcept { return creator_; }
    /// True if the creator is present.
    bool has_creator() const noexcept { return creator_.has_value(); }
    /// The creator's plugin component. It is empty if the creator is absent or unqualified.
    identifier_or_empty const& plugin() const noexcept;
    /// The creator's algorithm component, or nullptr if the creator is absent.
    identifier const* algorithm() const noexcept
    {
      return creator_ ? &creator_->algorithm() : nullptr;
    }
    /// The suffix: `empty_string` or a named `identifier`.
    suffix_t const& suffix() const noexcept { return suffix_; }
    type_id type() const noexcept { return type_id_; }

    void set_type(type_id&& type) { type_id_ = std::move(type); }

    auto operator<=>(product_specification const&) const = default;

    /// Parses `creator/suffix` or suffix-only text. Suffix-only text (`hits`, `/hits`, `""`, and
    /// `/`) yields an absent creator. A non-empty creator part must be a valid creator name;
    /// otherwise this throws `std::runtime_error`. Empty suffix text yields an explicit
    /// `empty_string` suffix; non-empty suffix text yields a named `identifier`.
    static product_specification create(char const* c);
    static product_specification create(std::string_view s);

    friend struct std::hash<product_specification>;

  private:
    std::optional<experimental::algorithm_name> creator_;
    experimental::suffix_t suffix_; // Default suffix is explicitly empty
    type_id type_id_{};
  };

  using product_specifications = std::vector<product_specification>;

}

namespace phlex::detail {
  PHLEX_MODEL_EXPORT experimental::product_specifications to_product_specifications(
    experimental::algorithm_name const& algo_name,
    std::vector<std::string> output_suffixes,
    type_ids output_types);
}

template <>
struct std::hash<phlex::experimental::product_specification> {
  std::size_t operator()(phlex::experimental::product_specification const& spec) const noexcept
  {
    // The creator's presence is part of the hash so that an absent creator is distinguished
    // from a present one, consistently with equality.
    std::size_t hash = spec.creator_.has_value() ? 1 : 0;
    if (spec.creator_) {
      boost::hash_combine(hash, spec.creator_->plugin().hash());
      boost::hash_combine(hash, spec.creator_->algorithm().hash());
    }
    boost::hash_combine(hash, spec.suffix_.hash());
    boost::hash_combine(hash, phlex::experimental::hash_value(spec.type_id_));
    return hash;
  }
};
#endif // PHLEX_MODEL_PRODUCT_SPECIFICATION_HPP
