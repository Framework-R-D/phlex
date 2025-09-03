#include "phlex_toy_config.hpp"

namespace mock_phlex::config {

  void parse_config::addItem(const std::string& product_name,
                             const std::string& file_name,
                             int technology)
  {
    m_items.emplace_back(product_name, file_name, technology);
  }

  const PersistenceItem* parse_config::findItem(const std::string& product_name) const
  {
    for (const auto& item : m_items) {
      if (item.product_name == product_name) {
        return &item;
      }
    }
    return nullptr;
  }

  void parse_config::addFileSetting(const int tech,
                                    const std::string& fileName,
                                    const std::string& key,
                                    const std::string& value)
  {
    m_file_settings[tech][fileName].emplace_back(key, value);
  }

  void parse_config::addContainerSetting(const int tech,
                                         const std::string& containerName,
                                         const std::string& key,
                                         const std::string& value)
  {
    m_container_settings[tech][containerName].emplace_back(key, value);
  }

} // namespace mock_phlex::config
