#ifndef __FORM_PARSE_CONFIG_HPP__
#define __FORM_PARSE_CONFIG_HPP__

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace form::experimental::config {

  struct PersistenceItem {
    std::string product_name; // e.g. "trackStart", "trackNumberHits"
    std::string file_name;    // e.g. "toy.root", "output.hdf5"
    int technology;           // Technology::ROOT_TTREE, Technology::ROOT_RNTUPLE, Technology::HDF5

    PersistenceItem() = default;

    PersistenceItem(const std::string& product, const std::string& file, int tech) :
      product_name(product), file_name(file), technology(tech)
    {
    }
  };

  class parse_config {
  public:
    parse_config() = default;
    ~parse_config() = default;

    // Add a configuration item
    void addItem(const std::string& product_name, const std::string& file_name, int technology);

    // Find configuration for a product+creator combination
    std::optional<PersistenceItem> findItem(const std::string& product_name) const;

    // Get all items (for debugging/validation)
    const std::vector<PersistenceItem>& getItems() const { return m_items; }

  private:
    std::vector<PersistenceItem> m_items;
  };

} // namespace form::experimental::config

#endif
