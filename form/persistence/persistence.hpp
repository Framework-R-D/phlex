// Copyright (C) 2025 ...

#ifndef FORM_PERSISTENCE_PERSISTENCE_HPP
#define FORM_PERSISTENCE_PERSISTENCE_HPP

#include "ipersistence.hpp"

#include "core/placement.hpp"
#include "core/token.hpp"
#include "storage/istorage.hpp"

#include <map>
#include <memory>
#include <string>

// forward declaration for form config
namespace form::experimental::config {
  class output_item_config;
  struct tech_setting_config;
}

namespace form::detail::experimental {

  class Persistence : public IPersistence {
  public:
    Persistence();
    ~Persistence() = default;
    void configureTechSettings(
      form::experimental::config::tech_setting_config const& tech_config_settings) override;

    void configureOutputItems(
      form::experimental::config::output_item_config const& output_items) override;

    void createContainers(std::string const& creator,
                          std::map<std::string, std::type_info const*> const& products) override;
    void registerWrite(std::string const& creator,
                       std::string const& label,
                       void const* data,
                       std::type_info const& type) override;
    void commitOutput(std::string const& creator, std::string const& id) override;

    void read(std::string const& creator,
              std::string const& label,
              std::string const& id,
              void const** data,
              std::type_info const& type) override;

  private:
    std::unique_ptr<Placement> getPlacement(std::string const& creator, std::string const& label);
    std::unique_ptr<Token> getToken(std::string const& creator,
                                    std::string const& label,
                                    std::string const& id);

    form::experimental::config::PersistenceItem const* findConfigItem(
      std::string const& label) const;
    std::string buildFullLabel(std::string_view creator, std::string_view label) const;

  private:
    std::unique_ptr<IStorage> m_store;
    form::experimental::config::output_item_config m_output_items;
    form::experimental::config::tech_setting_config m_tech_settings;
  };

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_PERSISTENCE_HPP
