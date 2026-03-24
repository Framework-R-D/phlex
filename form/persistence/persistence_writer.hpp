// Copyright (C) 2025 ...

#ifndef FORM_PERSISTENCE_PERSISTENCE_WRITER_HPP
#define FORM_PERSISTENCE_PERSISTENCE_WRITER_HPP

#include "ipersistence_writer.hpp"

#include "core/placement.hpp"
#include "storage/istorage.hpp"

#include <map>
#include <memory>
#include <string>

// forward declaration for form config
namespace form::experimental::config {
  class ItemConfig;
  struct tech_setting_config;
}

namespace form::detail::experimental {

  class PersistenceWriter : public IPersistenceWriter {
  public:
    PersistenceWriter();
    ~PersistenceWriter() = default;
    void configureTechSettings(
      form::experimental::config::tech_setting_config const& tech_config_settings) override;

    void configure(
      form::experimental::config::ItemConfig const& config_items) override;

    void createContainers(std::string const& creator,
                          std::map<std::string, std::type_info const*> const& products) override;
    void registerWrite(std::string const& creator,
                       std::string const& label,
                       void const* data,
                       std::type_info const& type) override;
    void commitOutput(std::string const& creator, std::string const& id) override;

  private:
    std::unique_ptr<Placement> getPlacement(std::string const& creator, std::string const& label);

  private:
    std::unique_ptr<IStorageWriter> m_store_writer;
    form::experimental::config::ItemConfig m_config_items;
    form::experimental::config::tech_setting_config m_tech_settings;
  };

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_PERSISTENCE_WRITER_HPP
