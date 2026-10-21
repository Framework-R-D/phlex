#ifndef PHLEX_MODEL_DATA_PRODUCT_CONCEPT_HPP
#define PHLEX_MODEL_DATA_PRODUCT_CONCEPT_HPP

#include "phlex/model/identifier.hpp"
#include "phlex/phlex_model_export.hpp"

#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_set>
#include <vector>

namespace phlex::experimental {

  // Type alias for std::type_index, representing a concrete data product type.
  using concrete_product_id = std::type_index;

  // Represents a data product concept.
  //
  // A data product concept is a named category that groups related concrete
  // data product types. For example, "hit" could be a concept that encompasses
  // multiple hit types (HitV1, HitV2, etc.).
  //
  // Each data_product_concept has:
  // - A unique name
  // - A set of concrete types that model this concept
  class PHLEX_MODEL_EXPORT data_product_concept {
  public:
    explicit data_product_concept(std::string name);

    std::string const& name() const noexcept;

    std::unordered_set<concrete_product_id> const& concrete_types() const noexcept;

    void add_concrete_type(concrete_product_id type);

    void add_concrete_types(std::unordered_set<concrete_product_id> const& types);

    bool has_concrete_type(concrete_product_id type) const;

    bool operator==(data_product_concept const&) const = default;

  private:
    std::string name_;
    std::unordered_set<concrete_product_id> concrete_types_;
  };

} // namespace phlex::experimental

#endif // PHLEX_MODEL_DATA_PRODUCT_CONCEPT_HPP
