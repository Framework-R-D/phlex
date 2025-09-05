#ifndef __PARSE_CONFIG_HPP__
#define __PARSE_CONFIG_HPP__

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace mock_phlex::config {

  struct PersistenceItem {
    std::string product_name; // e.g. "trackStart", "trackNumberHits"
    std::string file_name;    // e.g. "toy.root", "output.hdf5"
    int technology;           // Technology::ROOT_TTREE, Technology::ROOT_RNTUPLE, Technology::HDF5

    PersistenceItem(std::string const& product, std::string const& file, int tech) :
      product_name(product), file_name(file), technology(tech)
    {
    }
  };

  class parse_config {
  public:
    parse_config() = default;
    ~parse_config() = default;

    // Add a configuration item
    void addItem(std::string const& product_name, std::string const& file_name, int technology);
    void addFileSetting(int const tech,
                        std::string const& fileName,
                        std::string const& key,
                        std::string const& value);
    void addContainerSetting(int const tech,
                             std::string const& containerName,
                             std::string const& key,
                             std::string const& value);

    // Find configuration for a product+creator combination
    PersistenceItem const* findItem(std::string const& product_name) const;

    // Get all items (for debugging/validation)
    std::vector<PersistenceItem> const& getItems() const { return m_items; }
    auto const& getFileSettings() const { return m_file_settings; }
    auto const& getContainerSettings() const { return m_container_settings; }

  private:
    std::vector<PersistenceItem> m_items;

    using table_t = std::vector<std::pair<std::string, std::string>>;
    using map_t = std::map<int, std::unordered_map<std::string, table_t>>;
    map_t m_file_settings;
    map_t m_container_settings;
  };

} // namespace mock_phlex::config

#endif
