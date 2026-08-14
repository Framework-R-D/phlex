#include "phlex/model/data_product_concept.hpp"

#include <stdexcept>
#include <typeinfo>

namespace phlex::experimental {

  data_product_concept::data_product_concept(std::string name) : name_(std::move(name))
  {
    if (name_.empty()) {
      throw std::invalid_argument("Data product concept name cannot be empty");
    }
  }

  std::string const& data_product_concept::name() const noexcept { return name_; }

  std::unordered_set<concrete_product_id> const& data_product_concept::concrete_types()
    const noexcept
  {
    return concrete_types_;
  }

  void data_product_concept::add_concrete_type(concrete_product_id type)
  {
    concrete_types_.insert(type);
  }

  void data_product_concept::add_concrete_types(
    std::unordered_set<concrete_product_id> const& types)
  {
    concrete_types_.insert(types.begin(), types.end());
  }

  bool data_product_concept::has_concrete_type(concrete_product_id type) const
  {
    return concrete_types_.find(type) != concrete_types_.end();
  }

} // namespace phlex::experimental
