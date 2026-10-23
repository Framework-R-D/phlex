#ifndef PHLEX_MODEL_PRODUCT_SPECIFICATION_HPP
#define PHLEX_MODEL_PRODUCT_SPECIFICATION_HPP

#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/type_id.hpp"
#include "phlex/phlex_model_export.hpp"

#include <boost/container_hash/hash.hpp>

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
  /// The creator is a required `algorithm_name`; there is no default or creator-less
  /// construction. Equality, ordering, and hashing cover the creator, suffix, and type. A
  /// specification whose type is unspecified is an exact key, not a wildcard.
  ///
  /// The suffix is a `suffix_t`: either the explicit empty value (`empty_string`) or a named
  /// `identifier`. Parsing, defaulting, formatting, equality, ordering, and hashing all preserve
  /// that representation without manufacturing an empty `identifier`. The default suffix is
  /// explicitly empty. It is never an omitted selection criterion, which is `std::nullopt` in a
  /// product selector.
  ///
  /// Textual boundary: specifications supplied as text (the `creator/suffix` text accepted by
  /// the text constructors, and the strings given to the fluent `output_product_suffixes(...)`
  /// registration
  /// API, which `phlex::detail::to_product_specifications()` converts) remain strings. Empty
  /// suffix text becomes `empty_string` and non-empty suffix text becomes a named `identifier`,
  /// when the specification is created. Non-empty text that is not a valid name is rejected rather
  /// than stored as an empty suffix.
  ///
  /// This source-provider construction API is experimental and may change before it is
  /// promoted to the stable phlex namespace.
  class PHLEX_MODEL_EXPORT product_specification {
  public:
    // There is no default or creator-less construction: every specification names its creator.

    /// Parses `creator/suffix` text, which must contain the slash; `creator/` has an empty
    /// suffix. Text without a slash, with an empty creator part (`/hits`, `/`), or empty is
    /// invalid and throws `std::runtime_error`; an invalid creator or suffix throws
    /// `std::invalid_argument`. Empty suffix text yields an explicit `empty_string` suffix;
    /// non-empty suffix text yields a named `identifier`.
    // NOLINTBEGIN(google-explicit-constructor) - Implicit conversion is intentional
    product_specification(char const* name);
    product_specification(std::string const& name);
    product_specification(std::string_view name);
    // NOLINTEND(google-explicit-constructor)
    product_specification(algorithm_name creator, suffix_t suffix, type_id type);

    /// Text of the specification in the form `creator/suffix`; the type is not part of the text.
    std::string to_string() const;
    /// The creator; always present.
    algorithm_name const& creator() const noexcept { return creator_; }
    /// The suffix: `empty_string` or a named `identifier`.
    suffix_t const& suffix() const noexcept { return suffix_; }
    type_id type() const noexcept { return type_id_; }

    /// Sets, replaces, or clears (with a default-constructed `type_id`) the type.
    void set_type(type_id&& type) { type_id_ = std::move(type); }

    auto operator<=>(product_specification const&) const = default;

    friend struct std::hash<product_specification>;

  private:
    experimental::algorithm_name creator_;
    experimental::suffix_t suffix_;
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
    // Hash the same components that equality compares, including whether the plugin is present.
    std::size_t hash = 0;
    auto const& plugin = spec.creator_.plugin();
    boost::hash_combine(hash, plugin.has_value());
    if (plugin) {
      boost::hash_combine(hash, plugin->hash());
    }
    boost::hash_combine(hash, spec.creator_.algorithm().hash());
    boost::hash_combine(hash, spec.suffix_.hash());
    boost::hash_combine(hash, phlex::experimental::hash_value(spec.type_id_));
    return hash;
  }
};
#endif // PHLEX_MODEL_PRODUCT_SPECIFICATION_HPP
