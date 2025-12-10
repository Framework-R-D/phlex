#ifndef TEST_HELPERS_HPP
#define TEST_HELPERS_HPP

#include "form/form.hpp"
#include "data_products/track_start.hpp"

#include <memory>
#include <typeindex>
#include <string>

namespace form::experimental {
  
  // Helper to register a single type
  template<typename T>
  void registerType(product_type_names& map, const std::string& name) {
    map.names[std::type_index(typeid(T))] = name;
  }
  
  // Register vector types automatically
  template<typename T>
  void registerVectorType(product_type_names& map, const std::string& base_name) {
    map.names[std::type_index(typeid(T))] = base_name;
    map.names[std::type_index(typeid(std::vector<T>))] = "std::vector<" + base_name + ">";
  }
  
  inline std::shared_ptr<product_type_names> createTypeMap() {
    auto type_map = std::make_shared<product_type_names>();
    registerVectorType<TrackStart>(*type_map, "TrackStart");
    
    // Register all your data product types here
    // Easy to add more in the future:
    // registerVectorType<TrackStart>(*type_map, "TrackStart");
    // registerVectorType<HitCollection>(*type_map, "HitCollection");
    // registerVectorType<Vertex>(*type_map, "Vertex");
    
    return type_map;
  }
}

#endif
