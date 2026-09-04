#ifndef FORM_FORM_CONFIG_HPP
#define FORM_FORM_CONFIG_HPP

#include "core/technology.hpp"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace form::experimental::config {

  struct persistence_item {
    std::string product_name;    // e.g. "trackStart", "trackNumberHits"
    std::string file_name;       // e.g. "toy.root", "output.hdf5"
    technology::id technology{}; // technology::root_ttree, root_rntuple, hdf5

    persistence_item() = default;

    persistence_item(std::string product, std::string file, technology::id tech) :
      product_name(std::move(product)), file_name(std::move(file)), technology(tech)
    {
    }
  };

  class item_config {
  public:
    item_config() = default;
    ~item_config() = default;

    // Add a configuration item
    void add_item(std::string const& product_name,
                  std::string const& file_name,
                  technology::id technology);

    // Find configuration for a product+creator combination
    std::optional<persistence_item> find_item(std::string const& product_name) const;

    // Get all items (for debugging/validation)
    std::vector<persistence_item> const& get_items() const { return items_; }

  private:
    std::vector<persistence_item> items_;
  };

  struct tech_setting_config {
    using table_t = std::vector<std::pair<std::string, std::string>>;
    using map_t = std::map<technology::id, std::unordered_map<std::string, table_t>>;
    map_t file_settings;
    map_t container_settings;

    table_t get_file_table(technology::id technology, std::string const& file_name) const;
    table_t get_container_table(technology::id technology, std::string const& container_name) const;
  };

} // namespace form::experimental::config

#endif // FORM_FORM_CONFIG_HPP
