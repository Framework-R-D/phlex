#ifndef FORM_FORM_CONFIG_HPP
#define FORM_FORM_CONFIG_HPP

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace form::experimental::config {

  struct PersistenceItem {
    std::string product_name; // e.g. "trackStart", "trackNumberHits"
    std::string file_name;    // e.g. "toy.root", "output.hdf5"
    int technology;           // Technology::ROOT_TTREE, Technology::ROOT_RNTUPLE, Technology::HDF5

    PersistenceItem() = default;

    PersistenceItem(std::string const& product, std::string const& file, int tech) :
      product_name(product), file_name(file), technology(tech)
    {
    }
  };

  class output_item_config {
  public:
    output_item_config() = default;
    ~output_item_config() = default;

    // Add a configuration item
    void addItem(std::string const& product_name, std::string const& file_name, int technology);

    // Find configuration for a product+creator combination
    std::optional<PersistenceItem> findItem(std::string const& product_name) const;

    // Get all items (for debugging/validation)
    std::vector<PersistenceItem> const& getItems() const { return m_items; }

  private:
    std::vector<PersistenceItem> m_items;
  };

  struct tech_setting_config {
    using table_t = std::vector<std::pair<std::string, std::string>>;
    using map_t = std::map<int, std::unordered_map<std::string, table_t>>;
    map_t file_settings;
    map_t container_settings;

    table_t getFileTable(int const technology, std::string const& fileName) const;
    table_t getContainerTable(int const technology, std::string const& containerName) const;
  };

} // namespace form::experimental::config

#endif // FORM_FORM_CONFIG_HPP
