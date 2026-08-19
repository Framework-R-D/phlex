// Copyright (C) 2025 ...

#ifndef FORM_PERSISTENCE_IPERSISTENCE_WRITER_HPP
#define FORM_PERSISTENCE_IPERSISTENCE_WRITER_HPP

#include "core/token.hpp"

#include <map>
#include <memory>
#include <string>
#include <typeinfo>

namespace form::experimental::config {
  class ItemConfig;
  struct tech_setting_config;
}

namespace form::detail::experimental {

  class IPersistenceWriter {
  public:
    IPersistenceWriter() = default;
    virtual ~IPersistenceWriter() = default;

    virtual void configureTechSettings(
      form::experimental::config::tech_setting_config const& tech_config_settings) = 0;

    virtual void configure(form::experimental::config::ItemConfig const& configItems) = 0;

    virtual void createContainers(std::string const& creator,
                                  std::map<std::string, std::type_info const*> const& products) = 0;
    // Write one product and return a Token locating it: placement plus 0-based row (entry) number
    // If the backend does not address rows, the returned Token has no id set (hasId() == false).
    virtual Token registerWrite(std::string const& creator,
                                std::string const& label,
                                void const* data,
                                std::type_info const& type) = 0;
    virtual void commitOutput(std::string const& creator, std::string const& id) = 0;
  };

  std::unique_ptr<IPersistenceWriter> createPersistenceWriter();

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_IPERSISTENCE_WRITER_HPP
