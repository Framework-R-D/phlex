#include "config.hpp"

namespace phlex::config {

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

} // namespace phlex::config
