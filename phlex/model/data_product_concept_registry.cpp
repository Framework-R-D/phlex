#include "phlex/model/data_product_concept_registry.hpp"

#include "phlex/model/data_product_concept.hpp"

#include <algorithm>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace phlex::experimental {

  concept_status data_product_concept_registry::register_concept(
    std::unique_ptr<data_product_concept> con)
  {
    if (!con) {
      throw std::invalid_argument{"Cannot register null concept"};
    }
    auto name = con->name(); // Copy the name before moving from con.

    std::unique_lock const lock{mutex_};
    if (auto* existing = find_concept_unlocked(name)) {
      auto const previous_size = existing->concrete_types().size();
      existing->add_concrete_types(con->concrete_types());
      return existing->concrete_types().size() == previous_size ? concept_status::unchanged
                                                                : concept_status::updated;
    }
    concepts_.emplace(std::move(name), std::move(con));
    return concept_status::added;
  }

  concept_status data_product_concept_registry::add_concrete_type(std::string const& name,
                                                                  concrete_product_id type)
  {
    std::unique_lock const lock{mutex_};
    if (auto* existing = find_concept_unlocked(name)) {
      if (existing->has_concrete_type(type)) {
        return concept_status::unchanged;
      }
      existing->add_concrete_type(type);
      return concept_status::updated;
    }

    auto con = std::make_unique<data_product_concept>(name);
    con->add_concrete_type(type);
    concepts_.emplace(name, std::move(con));
    return concept_status::added;
  }

  data_product_concept* data_product_concept_registry::find_concept_unlocked(
    std::string const& name) const
  {
    auto it = concepts_.find(name);
    if (it != concepts_.end()) {
      return it->second.get();
    }
    return nullptr;
  }

  data_product_concept const* data_product_concept_registry::find_concept(
    std::string const& name) const
  {
    std::shared_lock const lock{mutex_};
    return find_concept_unlocked(name);
  }

  data_product_concept* data_product_concept_registry::find_concept(std::string const& name)
  {
    std::shared_lock const lock{mutex_};
    return find_concept_unlocked(name);
  }

  std::vector<std::string> data_product_concept_registry::all_concept_names() const
  {
    std::shared_lock const lock{mutex_};
    std::vector<std::string> names;
    names.reserve(concepts_.size());
    for (auto const& [name, _] : concepts_) {
      names.push_back(name);
    }
    std::ranges::sort(names);
    return names;
  }

  bool data_product_concept_registry::type_matches_concept(std::string const& concept_name,
                                                           concrete_product_id type) const
  {
    std::shared_lock const lock{mutex_};
    auto const* con = find_concept_unlocked(concept_name);
    if (!con) {
      return false;
    }
    return con->has_concrete_type(type);
  }

} // namespace phlex::experimental
