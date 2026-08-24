// Copyright (C) 2025 ...

#ifndef FORM_PERSISTENCE_IPERSISTENCE_WRITER_HPP
#define FORM_PERSISTENCE_IPERSISTENCE_WRITER_HPP

#include "core/token.hpp"

#include <map>
#include <memory>
#include <string>
#include <typeinfo>

namespace form::experimental::config {
  class item_config;
  struct tech_setting_config;
}

namespace form::detail::experimental {

  class i_persistence_writer {
  public:
    i_persistence_writer() = default;
    virtual ~i_persistence_writer() = default;

    virtual void configure_tech_settings(
      form::experimental::config::tech_setting_config const& tech_config_settings) = 0;

    virtual void configure(form::experimental::config::item_config const& config_items) = 0;

    virtual void create_containers(
      std::string const& creator, std::map<std::string, std::type_info const*> const& products) = 0;
    // Write one product and return a token locating it: placement plus 0-based row (entry) number
    // Throws if backend isn't row-addressed, causing token read lookup to fail
    virtual token register_write(std::string const& creator,
                                 std::string const& label,
                                 void const* data,
                                 std::type_info const& type) = 0;
    virtual void commit_output(std::string const& creator, std::string const& id) = 0;
  };

  std::unique_ptr<i_persistence_writer> create_persistence_writer();

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_IPERSISTENCE_WRITER_HPP
