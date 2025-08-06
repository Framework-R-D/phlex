#ifndef __PARSE_CONFIG_H__
#define __PARSE_CONFIG_H__

#include <memory>
#include <string>
#include <vector>

namespace phlex::config {

  // Technology constants
  namespace Technology {
    const int ROOT_TTREE = 1 * 256 + 1;   // ROOT TTree
    const int ROOT_RNTUPLE = 1 * 256 + 2; // ROOT RNtuple
    const int HDF5 = 2 * 256 + 1;         // HDF5
  }

  struct PersistenceItem {
    std::string product_name; // e.g. "trackStart", "trackNumberHits"
    std::string file_name;    // e.g. "toy.root", "output.hdf5"
    int technology;           // Technology::ROOT_TTREE, Technology::ROOT_RNTUPLE, Technology::HDF5

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
    const PersistenceItem* findItem(const std::string& product_name) const;

    // Get all items (for debugging/validation)
    const std::vector<PersistenceItem>& getItems() const { return m_items; }

  private:
    std::vector<PersistenceItem> m_items;
  };

} // namespace phlex::config

#endif
