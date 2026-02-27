// Copyright (C) 2025 ...

#ifndef FORM_PERSISTENCE_IPERSISTENCE_READER_HPP
#define FORM_PERSISTENCE_IPERSISTENCE_READER_HPP

#include <map>
#include <memory>
#include <string>
#include <typeinfo>

namespace form::experimental::config {
  class output_item_config;
  struct tech_setting_config;
}

namespace form::detail::experimental {

  class IPersistenceReader {
  public:
    IPersistenceReader() {};
    virtual ~IPersistenceReader() = default;

    virtual void configureTechSettings(
      form::experimental::config::tech_setting_config const& tech_config_settings) = 0;

    virtual void configureOutputItems(
      form::experimental::config::output_item_config const& outputItems) = 0;

    virtual void read(std::string const& creator,
                      std::string const& label,
                      std::string const& id,
                      void const** data,
                      std::type_info const& type) = 0;
  };

  std::unique_ptr<IPersistenceReader> createPersistenceReader();

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_IPERSISTENCE_READER_HPP
