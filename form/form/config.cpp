#include "form/config.hpp"

namespace {
  template <class MapLike>
  auto const_lookup(MapLike const& map, typename MapLike::key_type const& key)
  {
    auto const found = map.find(key);
    if (found != map.end()) {
      return found->second;
    }
    return decltype(found->second)();
  }
}

namespace form::experimental::config {

  void item_config::add_item(std::string const& product_name,
                             std::string const& file_name,
                             technology::id technology)
  {
    items_.emplace_back(product_name, file_name, technology);
  }

  std::optional<persistence_item> item_config::find_item(std::string const& product_name) const
  {
    for (auto const& item : items_) {
      if (item.product_name == product_name) {
        return item;
      }
    }
    return std::nullopt;
  }

  tech_setting_config::table_t tech_setting_config::get_file_table(
    technology::id const technology, std::string const& file_name) const
  {
    auto const per_tech = ::const_lookup(file_settings, technology);
    return ::const_lookup(per_tech, file_name);
  }

  tech_setting_config::table_t tech_setting_config::get_container_table(
    technology::id const technology, std::string const& container_name) const
  {
    auto const per_tech = ::const_lookup(container_settings, technology);
    return ::const_lookup(per_tech, container_name);
  }

} // namespace form::experimental::config
