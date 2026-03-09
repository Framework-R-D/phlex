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

namespace phlex::experimental {
  class phlex_model_EXPORT product_specification {
  public:
    product_specification();
    product_specification(char const* name);
    product_specification(std::string const& name);
    product_specification(std::string_view name);
    product_specification(algorithm_name qualifier, identifier name, type_id type);

    std::string full() const;
    algorithm_name const& qualifier() const noexcept { return qualifier_; }
    identifier const& plugin() const noexcept { return qualifier_.plugin(); }
    identifier const& algorithm() const noexcept { return qualifier_.algorithm(); }
    identifier const& name() const noexcept { return name_; }
    type_id type() const noexcept { return type_id_; }

    void set_type(type_id&& type) { type_id_ = std::move(type); }

    auto operator<=>(product_specification const&) const = default;

    static product_specification create(char const* c);
    static product_specification create(std::string_view s);

    friend struct std::hash<product_specification>;

  private:
    algorithm_name qualifier_;
    identifier name_;
    type_id type_id_{};
  };

  using product_specifications = std::vector<product_specification>;

  phlex_model_EXPORT product_specifications
  to_product_specifications(std::string_view name,
                            std::vector<std::string> output_labels,
                            std::vector<type_id> output_types);
}

template <>
struct std::hash<phlex::experimental::product_specification> {
  std::size_t operator()(phlex::experimental::product_specification const& spec) const noexcept
  {
    std::size_t hash = spec.qualifier_.plugin().hash();
    boost::hash_combine(hash, spec.qualifier_.algorithm().hash());
    boost::hash_combine(hash, spec.name_.hash());
    boost::hash_combine(hash, spec.type_id_);
    return hash;
  }
};
#endif // PHLEX_MODEL_PRODUCT_SPECIFICATION_HPP
