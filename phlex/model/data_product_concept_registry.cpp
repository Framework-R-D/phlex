#include "phlex/model/data_product_concept_registry.hpp"

#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace phlex::experimental {

  bool data_product_concept_registry::register_concept(std::unique_ptr<data_product_concept> con)
  {
    if (!con) {
      throw std::invalid_argument{"Cannot register null concept"};
    }
    auto name = con->name(); // Copy the name before moving from con.

    std::unique_lock lock{mutex_};
    if (auto* existing = find_concept_unlocked(name)) {
      existing->add_concrete_types(con->concrete_types());
      return false;
    }
    concepts_.emplace(std::move(name), std::move(con));
    return true;
  }

  bool data_product_concept_registry::add_concrete_type(std::string const& name,
                                                         concrete_product_id type)
  {
    std::unique_lock lock{mutex_};
    if (auto* existing = find_concept_unlocked(name)) {
      existing->add_concrete_type(type);
      return false;
    }

    auto con = std::make_unique<data_product_concept>(name);
    con->add_concrete_type(type);
    concepts_.emplace(name, std::move(con));
    return true;
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
    std::shared_lock lock{mutex_};
    return find_concept_unlocked(name);
  }

  data_product_concept* data_product_concept_registry::find_concept(std::string const& name)
  {
    std::shared_lock lock{mutex_};
    return find_concept_unlocked(name);
  }

  std::vector<std::string> data_product_concept_registry::all_concept_names() const
  {
    std::shared_lock lock{mutex_};
    std::vector<std::string> names;
    names.reserve(concepts_.size());
    for (auto const& [name, _] : concepts_) {
      names.push_back(name);
    }
    std::sort(names.begin(), names.end());
    return names;
  }

  bool data_product_concept_registry::type_matches_concept(std::string const& concept_name,
                                                           concrete_product_id type) const
  {
    std::shared_lock lock{mutex_};
    auto const* con = find_concept_unlocked(concept_name);
    if (!con) {
      return false;
    }
    return con->has_concrete_type(type);
  }

  data_product_concept const* data_product_concept_registry::get_concept_for_type(
    concrete_product_id type) const
  {
    std::shared_lock lock{mutex_};
    for (auto const& [name, con] : concepts_) {
      if (con->has_concrete_type(type)) {
        return con.get();
      }
    }
    return nullptr;
  }

} // namespace phlex::experimental
