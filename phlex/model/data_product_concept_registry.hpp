#ifndef PHLEX_MODEL_DATA_PRODUCT_CONCEPT_REGISTRY_HPP
#define PHLEX_MODEL_DATA_PRODUCT_CONCEPT_REGISTRY_HPP

#include "phlex/model/data_product_concept.hpp"
#include "phlex/phlex_model_export.hpp"

#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace phlex::experimental {

  enum class concept_status { added, updated };

  // Registry for data product concepts.
  //
  // This registry stores data_product_concept instances and allows
  // lookup by name. Each concept can be associated with one or more
  // concrete data product types.
  //
  // All public operations are individually safe for concurrent invocation:
  // a registry-level std::shared_mutex protects the map and every access to
  // a stored concept's concrete-type set that is made through the registry's
  // own methods. This guarantee does not extend past the return of a call.
  // In particular, find_concept returns a pointer to a stored concept; the
  // caller must not dereference or mutate through that pointer concurrently
  // with a registry write, or concurrently with another caller's mutation via
  // a similarly obtained pointer, since such access is not covered by the
  // registry's lock.
  class PHLEX_MODEL_EXPORT data_product_concept_registry {
  public:
    data_product_concept_registry() = default;
    ~data_product_concept_registry() = default;

    // NOTE: It is unclear whether both register_concept and add_concrete_type
    // are necessary. In later revisions of the code we will try to make all
    // manipulations of data_product_concept instances be private to the
    // data_product_concept_registry.
    //
    // Register a data product concept.
    //
    // Returns concept_status::added if a new concept was inserted, or
    // concept_status::updated if a concept with the same name was already
    // registered. In the latter case, the incoming concept's concrete types
    // are merged into the existing concept's set and the incoming concept is
    // discarded, even if the set was unchanged by the merge.
    //
    // Throws std::invalid_argument if con is null.
    concept_status register_concept(std::unique_ptr<data_product_concept> con);

    // Register a concrete data product type with the concept of the given name.
    // Returns concept_status::added if a new concept was registered, or
    // concept_status::updated if the concept already existed, even if the
    // type was already associated with it.
    concept_status add_concrete_type(std::string const& name, concrete_product_id type);

    // Find a data product concept by name.
    //
    // The returned pointer is not safe to dereference (or, for the
    // non-const overload, to mutate through) concurrently with a registry
    // write after this method returns.
    data_product_concept const* find_concept(std::string const& name) const;
    data_product_concept* find_concept(std::string const& name);

    // Get all registered concept names.
    std::vector<std::string> all_concept_names() const;

    // Check if a concrete type is associated with a concept.
    bool type_matches_concept(std::string const& concept_name, concrete_product_id type) const;

    // data_product_concept_registry objects can be neither copied nor moved.
    data_product_concept_registry(data_product_concept_registry const&) = delete;
    data_product_concept_registry& operator=(data_product_concept_registry const&) = delete;
    data_product_concept_registry(data_product_concept_registry&&) noexcept = delete;
    data_product_concept_registry& operator=(data_product_concept_registry&&) noexcept = delete;

  private:
    // Assumes the caller already holds mutex_ (shared or exclusive).
    // Performs a plain, unlocked lookup. This overload is const yet returns
    // a non-const pointer (legal, since the pointee is reached through
    // std::unique_ptr const&::get()); a single helper serves both the
    // const and non-const find_concept overloads, so the const overload's
    // constness is nominal only as far as the pointee is concerned.
    data_product_concept* find_concept_unlocked(std::string const& name) const;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<data_product_concept>> concepts_;
  };

} // namespace phlex::experimental

#endif // PHLEX_MODEL_DATA_PRODUCT_CONCEPT_REGISTRY_HPP
