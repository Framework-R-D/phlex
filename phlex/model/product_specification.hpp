#ifndef PHLEX_MODEL_PRODUCT_SPECIFICATION_HPP
#define PHLEX_MODEL_PRODUCT_SPECIFICATION_HPP

#include "phlex/phlex_model_export.hpp"

#include "phlex/model/algorithm_name.hpp"
#include "phlex/model/identifier.hpp"
#include "phlex/model/type_id.hpp"

#include "boost/container_hash/hash.hpp"

#include <string>
#include <utility>
#include <vector>

namespace phlex::detail {
  class PHLEX_MODEL_EXPORT product_specification {
  public:
    product_specification();
    // NOLINTBEGIN(google-explicit-constructor) - Implicit conversion is intentional
    product_specification(char const* name);
    product_specification(std::string const& name);
    product_specification(std::string_view name);
    // NOLINTEND(google-explicit-constructor)
    product_specification(experimental::algorithm_name creator,
                          experimental::identifier suffix,
                          type_id type);

    std::string to_string() const;
    experimental::algorithm_name const& creator() const noexcept { return creator_; }
    experimental::identifier const& plugin() const noexcept { return creator_.plugin(); }
    experimental::identifier const& algorithm() const noexcept { return creator_.algorithm(); }
    experimental::identifier const& suffix() const noexcept { return suffix_; }
    type_id type() const noexcept { return type_id_; }

    void set_type(type_id&& type) { type_id_ = std::move(type); }

    auto operator<=>(product_specification const&) const = default;

    static product_specification create(char const* c);
    static product_specification create(std::string_view s);

    friend struct std::hash<product_specification>;

  private:
    experimental::algorithm_name creator_;
    experimental::identifier suffix_; // Default suffix is empty string
    type_id type_id_{};
  };

  using product_specifications = std::vector<product_specification>;

  PHLEX_MODEL_EXPORT product_specifications
  to_product_specifications(experimental::algorithm_name const& algo_name,
                            std::vector<std::string> output_suffixes,
                            std::vector<type_id> output_types);
}

template <>
struct std::hash<phlex::detail::product_specification> {
  std::size_t operator()(phlex::detail::product_specification const& spec) const noexcept
  {
    std::size_t hash = spec.creator_.plugin().hash();
    boost::hash_combine(hash, spec.creator_.algorithm().hash());
    boost::hash_combine(hash, spec.suffix_.hash());
    boost::hash_combine(hash, spec.type_id_);
    return hash;
  }
};
#endif // PHLEX_MODEL_PRODUCT_SPECIFICATION_HPP
