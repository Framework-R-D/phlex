#ifndef TEST_HELPERS_HPP
#define TEST_HELPERS_HPP

#include "data_products/track_start.hpp"
#include "form/form.hpp"

#include <memory>
#include <string>
#include <typeindex>

namespace form::experimental {

  // Helper to register a single type
  template <typename T>
  void registerType(product_type_names& map, std::string const& name)
  {
    map.names[std::type_index(typeid(T))] = name;
  }

  // Register vector types automatically
  template <typename T>
  void registerVectorType(product_type_names& map, std::string const& base_name)
  {
    map.names[std::type_index(typeid(T))] = base_name;
    map.names[std::type_index(typeid(std::vector<T>))] = "std::vector<" + base_name + ">";
  }

  inline std::shared_ptr<product_type_names> createTypeMap()
  {
    auto type_map = std::make_shared<product_type_names>();
    registerVectorType<TrackStart>(*type_map, "TrackStart");

    // Register all your data product types here
    // Easy to add more in the future:
    return type_map;
  }
}

#endif
