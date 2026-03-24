// Copyright (C) 2025 ...

#ifndef FORM_PERSISTENCE_PERSISTENCE_READER_HPP
#define FORM_PERSISTENCE_PERSISTENCE_READER_HPP

#include "ipersistence_reader.hpp"

#include "core/token.hpp"
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

  class PersistenceReader : public IPersistenceReader {
  public:
    PersistenceReader();
    ~PersistenceReader() = default;
    void configureTechSettings(
      form::experimental::config::tech_setting_config const& tech_config_settings) override;

    void configureOutputItems(
      form::experimental::config::ItemConfig const& output_items) override;

    void read(std::string const& creator,
              std::string const& label,
              std::string const& id,
              void const** data,
              std::type_info const& type) override;

  private:
    std::unique_ptr<Token> getToken(std::string const& creator,
                                    std::string const& label,
                                    std::string const& id);

  private:
    std::unique_ptr<IStorageReader> m_store_reader;
    form::experimental::config::ItemConfig m_output_items;
    form::experimental::config::tech_setting_config m_tech_settings;
  };

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_PERSISTENCE_READER_HPP
