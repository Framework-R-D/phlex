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
  class item_config;
  struct tech_setting_config;
}

namespace form::detail::experimental {

  class persistence_reader : public i_persistence_reader {
  public:
    persistence_reader();
    ~persistence_reader() override = default;
    void configure_tech_settings(
      form::experimental::config::tech_setting_config const& tech_config_settings) override;

    void configure(form::experimental::config::item_config const& config_items) override;

    void read(std::string const& creator,
              std::string const& label,
              std::string const& id,
              void const** data,
              std::type_info const& type) override;

    void prime(std::string const& creator,
               std::string const& label,
               std::type_info const& type) override;

    std::vector<std::string> list_indices(std::string const& creator,
                                          std::string const& label) override;

  private:
    std::unique_ptr<token> get_token(std::string const& creator,
                                     std::string const& label,
                                     std::string const& id);

    std::unique_ptr<i_storage_reader> store_reader_;
    form::experimental::config::item_config config_items_;
    form::experimental::config::tech_setting_config tech_settings_;
  };

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_PERSISTENCE_READER_HPP
