// Copyright (C) 2025 ...

#ifndef __IPERSISTENCE_HPP__
#define __IPERSISTENCE_HPP__

#include <map>
#include <memory>
#include <string>

namespace form::experimental::config {
  class output_item_config;
  struct tech_setting_config;
}

namespace form::detail::experimental {

  class IPersistence {
  public:
    IPersistence() {};
    virtual ~IPersistence() = default;

    virtual void configureTechSettings(
      form::experimental::config::tech_setting_config const& tech_config_settings) = 0;

    virtual void configureOutputItems(
      form::experimental::config::output_item_config const& outputItems) = 0;

    virtual void createContainers(std::string const& creator,
                                  std::map<std::string, std::string> const& products) = 0;
    virtual void registerWrite(std::string const& creator,
                               std::string const& label,
                               void const* data,
                               std::string const& type) = 0;
    virtual void commitOutput(std::string const& creator, std::string const& id) = 0;

    virtual void read(std::string const& creator,
                      std::string const& label,
                      std::string const& id,
                      void const** data,
                      std::string& type) = 0;
  };

  std::unique_ptr<IPersistence> createPersistence();

} // namespace form::detail::experimental

#endif
