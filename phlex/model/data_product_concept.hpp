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
  // - A unique name, which can not be modified
  // - A set of concrete types that model this concept
  class PHLEX_MODEL_EXPORT data_product_concept {
  public:
    explicit data_product_concept(std::string name);

    // Copying preserves the source. Moving could remove types from a registered
    // concept, and assignment could replace its identity or type associations.
    data_product_concept(data_product_concept const&) = default;
    data_product_concept(data_product_concept&&) = delete;
    data_product_concept& operator=(data_product_concept const&) = delete;
    data_product_concept& operator=(data_product_concept&&) = delete;
    ~data_product_concept() = default;

    std::string const& name() const noexcept;

    std::unordered_set<concrete_product_id> const& concrete_types() const noexcept;

    void add_concrete_type(concrete_product_id type);

    void add_concrete_types(std::unordered_set<concrete_product_id> const& types);

    bool has_concrete_type(concrete_product_id type) const;

    bool operator==(data_product_concept const&) const = default;

  private:
    // The name must remain consistent with the registry key.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    std::string const name_;
    std::unordered_set<concrete_product_id> concrete_types_;
  };

} // namespace phlex::experimental

#endif // PHLEX_MODEL_DATA_PRODUCT_CONCEPT_HPP
