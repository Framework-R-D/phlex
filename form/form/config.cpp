#include "form/config.hpp"

namespace {
  template <class MAP_LIKE>
  auto const_lookup(const MAP_LIKE& map, typename MAP_LIKE::key_type const& key)
  {
    auto const found = map.find(key);
    if (found != map.end())
      return found->second;
    return decltype(found->second)();
  }
}

namespace form::experimental::config {

  void output_item_config::addItem(std::string const& product_name,
                                   std::string const& file_name,
                                   int technology)
  {
    m_items.emplace_back(product_name, file_name, technology);
  }

  std::optional<PersistenceItem> output_item_config::findItem(std::string const& product_name) const
  {
    for (auto const& item : m_items) {
      if (item.product_name == product_name) {
        return item;
      }
    }
    return std::nullopt;
  }

  tech_setting_config::table_t tech_setting_config::getFileTable(int const technology,
                                                                 std::string const& fileName) const
  {
    auto const per_tech = ::const_lookup(file_settings, technology);
    return ::const_lookup(per_tech, fileName);
  }

  tech_setting_config::table_t tech_setting_config::getContainerTable(
    int const technology, std::string const& containerName) const
  {
    auto const per_tech = ::const_lookup(container_settings, technology);
    return ::const_lookup(per_tech, containerName);
  }

} // namespace form::experimental::config
