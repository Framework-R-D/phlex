#include "phlex_toy_config.hpp"

namespace mock_phlex::config {

  void parse_config::addItem(std::string const& product_name,
                             std::string const& file_name,
                             int technology)
  {
    m_items.emplace_back(product_name, file_name, technology);
  }

  PersistenceItem const* parse_config::findItem(std::string const& product_name) const
  {
    for (auto const& item : m_items) {
      if (item.product_name == product_name) {
        return &item;
      }
    }
    return nullptr;
  }

} // namespace mock_phlex::config
