// Copyright (C) 2025 ...

#ifndef FORM_PERSISTENCE_IPERSISTENCE_READER_HPP
#define FORM_PERSISTENCE_IPERSISTENCE_READER_HPP

#include <map>
#include <memory>
#include <string>
#include <typeinfo>
#include <vector>

namespace form::experimental::config {
  class item_config;
  struct tech_setting_config;
}

namespace form::detail::experimental {

  class i_persistence_reader {
  public:
    i_persistence_reader() = default;
    virtual ~i_persistence_reader() = default;

    virtual void configure_tech_settings(
      form::experimental::config::tech_setting_config const& tech_config_settings) = 0;

    virtual void configure(form::experimental::config::item_config const& config_items) = 0;

    virtual void read(std::string const& creator,
                      std::string const& label,
                      std::string const& id,
                      void const** data,
                      std::type_info const& type) = 0;

    virtual void prime(std::string const& creator,
                       std::string const& label,
                       std::type_info const& type) = 0;

    virtual std::vector<std::string> list_indices(std::string const& creator,
                                                  std::string const& label) = 0;
  };

  std::unique_ptr<i_persistence_reader> create_persistence_reader();

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_IPERSISTENCE_READER_HPP
