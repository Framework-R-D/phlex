#ifndef PHLEX_MODEL_DATA_PRODUCT_CONCEPT_HPP
#define PHLEX_MODEL_DATA_PRODUCT_CONCEPT_HPP

#include "phlex/phlex_model_export.hpp"

#include "phlex/model/identifier.hpp"

#include <any>
#include <map>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <unordered_set>
#include <utility>
#include <vector>

namespace phlex::experimental {

  // Type alias for std::type_index, representing a concrete data product type.
  using concrete_product_id = std::type_index;

  // Declares the relationship between the result of a conversion function and
  // its storage.
  //
  // A conversion is 'owned' when its result owns its storage, and 'borrowed'
  // when its result refers to storage owned by the input product. A borrowed
  // result dangles if its source is released, so the framework must keep the
  // source alive; this cannot be deduced and must be declared at registration.
  enum class result_storage { owned, borrowed };

  // A conversion function registered with a data product concept, together
  // with the information a translator node needs in order to use it.
  struct translator_registration {
    // Name of the translator algorithm, recorded on products it produces.
    std::string name;

    // The conversion function, type-erased. How the function is recovered for
    // node creation is deliberately left open: nothing looks it up yet.
    std::any function;

    // Whether the conversion's result owns or borrows its storage.
    result_storage storage;
  };

  // Represents a data product concept.
  //
  // A data product concept is a named category that groups related concrete
  // data product types. For example, "hit" could be a concept that encompasses
  // multiple hit types (HitV1, HitV2, etc.).
  //
  // Each data_product_concept has:
  // - A unique name
  // - A set of concrete types that model this concept
  // - A set of conversion functions between pairs of those concrete types
  //
  // Registering a conversion function is the only way one can enter the
  // system, so the invariants checked by add_translator hold for every
  // translator in the system and need not be rechecked by its users.
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

    // Register a conversion function from the source to the target concrete
    // type, under the given translator name.
    //
    // Throws std::invalid_argument if:
    // - source and target are the same type,
    // - either type is not already a concrete type of this concept, or
    // - a translator is already registered for that ordered pair.
    void add_translator(std::string translator_name,
                        concrete_product_id source,
                        concrete_product_id target,
                        std::any function,
                        result_storage storage);

    // Get the translator registered for an ordered pair of concrete types, or
    // nullptr if there is none. The returned pointer is invalidated by the
    // destruction of this concept.
    translator_registration const* find_translator(concrete_product_id source,
                                                   concrete_product_id target) const;

    // Compares the name and the concrete types. Registered translators do not
    // participate: a std::any holding a conversion function is not
    // equality-comparable.
    bool operator==(data_product_concept const& other) const;

  private:
    using translator_key = std::pair<concrete_product_id, concrete_product_id>;

    std::string name_;
    std::unordered_set<concrete_product_id> concrete_types_;
    std::map<translator_key, translator_registration> translators_;
  };

} // namespace phlex::experimental

#endif // PHLEX_MODEL_DATA_PRODUCT_CONCEPT_HPP
