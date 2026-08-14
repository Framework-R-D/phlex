#ifndef PHLEX_MODEL_DATA_PRODUCT_CONCEPT_HPP
#define PHLEX_MODEL_DATA_PRODUCT_CONCEPT_HPP

#include "phlex/phlex_model_export.hpp"

#include "phlex/model/identifier.hpp"

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
    // Construct a data_product_concept with a name.
    explicit data_product_concept(std::string name);

    // Get the name of this concept.
    std::string const& name() const noexcept;

    // Get the set of concrete types that model this concept.
    std::unordered_set<concrete_product_id> const& concrete_types() const noexcept;

    // Add a concrete type to this concept.
    void add_concrete_type(concrete_product_id type);

    // Add a whole set of concrete types to this concept in one call.
    void add_concrete_types(std::unordered_set<concrete_product_id> const& types);

    // Check if a concrete type is associated with this concept.
    bool has_concrete_type(concrete_product_id type) const;

    bool operator==(data_product_concept const&) const = default;

  private:
    std::string name_;
    std::unordered_set<concrete_product_id> concrete_types_;
  };

} // namespace phlex::experimental

#endif // PHLEX_MODEL_DATA_PRODUCT_CONCEPT_HPP
