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
  class item_config;
  struct tech_setting_config;
}

namespace form::detail::experimental {

  class persistence_writer : public i_persistence_writer {
  public:
    persistence_writer();
    ~persistence_writer() override = default;
    void configure_tech_settings(
      form::experimental::config::tech_setting_config const& tech_config_settings) override;

    void configure(form::experimental::config::item_config const& config_items) override;

    void create_containers(std::string const& creator,
                           std::map<std::string, std::type_info const*> const& products) override;
    token register_write(std::string const& creator,
                         std::string const& label,
                         void const* data,
                         std::type_info const& type) override;
    void commit_output(std::string const& creator, std::string const& id) override;

  private:
    std::unique_ptr<placement> get_placement(std::string const& creator, std::string const& label);

    std::unique_ptr<i_storage_writer> store_writer_;
    form::experimental::config::item_config config_items_;
    form::experimental::config::tech_setting_config tech_settings_;
  };

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_PERSISTENCE_WRITER_HPP
