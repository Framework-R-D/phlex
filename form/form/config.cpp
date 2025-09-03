#include "form/config.hpp"

namespace {
  template <class MAP_LIKE>
  auto const_lookup(const MAP_LIKE& map, const typename MAP_LIKE::key_type& key)
  {
    const auto found = map.find(key);
    if (found != map.end())
      return found->second;
    return decltype(found->second)();
  }
}

namespace form::experimental::config {

  void output_item_config::addItem(const std::string& product_name,
                                   const std::string& file_name,
                                   int technology)
  {
    m_items.emplace_back(product_name, file_name, technology);
  }

  std::optional<PersistenceItem> output_item_config::findItem(const std::string& product_name) const
  {
    for (const auto& item : m_items) {
      if (item.product_name == product_name) {
        return item;
      }
    }
    return std::nullopt;
  }

  tech_setting_config::table_t tech_setting_config::getFileTable(const int technology,
                                                                 const std::string& fileName) const
  {
    const auto per_tech = ::const_lookup(file_settings, technology);
    return ::const_lookup(per_tech, fileName);
  }

  tech_setting_config::table_t tech_setting_config::getContainerTable(
    const int technology, const std::string& containerName) const
  {
    const auto per_tech = ::const_lookup(container_settings, technology);
    return ::const_lookup(per_tech, containerName);
  }

} // namespace form::config
