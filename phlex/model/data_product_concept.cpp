#include "phlex/model/data_product_concept.hpp"

#include "boost/core/demangle.hpp"
#include "fmt/format.h"

#include <stdexcept>
#include <typeinfo>
#include <utility>

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

  void data_product_concept::add_translator(std::string translator_name,
                                            concrete_product_id source,
                                            concrete_product_id target,
                                            std::any function,
                                            result_storage storage)
  {
    if (source == target) {
      throw std::invalid_argument{
        fmt::format("Cannot register translator '{}' with concept '{}': the source and target "
                    "types are both '{}'.",
                    translator_name,
                    name_,
                    boost::core::demangle(source.name()))};
    }

    for (auto type : {source, target}) {
      if (!has_concrete_type(type)) {
        throw std::invalid_argument{
          fmt::format("Cannot register translator '{}' with concept '{}': type '{}' is not a "
                      "concrete type of that concept.",
                      translator_name,
                      name_,
                      boost::core::demangle(type.name()))};
      }
    }

    translator_key const key{source, target};
    if (auto it = translators_.find(key); it != translators_.end()) {
      throw std::invalid_argument{
        fmt::format("Cannot register translator '{}' with concept '{}': translator '{}' is "
                    "already registered for the conversion from '{}' to '{}'.",
                    translator_name,
                    name_,
                    it->second.name,
                    boost::core::demangle(source.name()),
                    boost::core::demangle(target.name()))};
    }

    translators_.emplace(
      key, translator_registration{std::move(translator_name), std::move(function), storage});
  }

  translator_registration const* data_product_concept::find_translator(
    concrete_product_id source, concrete_product_id target) const
  {
    auto it = translators_.find(translator_key{source, target});
    if (it == translators_.end()) {
      return nullptr;
    }
    return &it->second;
  }

  bool data_product_concept::operator==(data_product_concept const& other) const
  {
    return name_ == other.name_ && concrete_types_ == other.concrete_types_;
  }

} // namespace phlex::experimental
