// Copyright (C) 2025 ...

#ifndef FORM_PERSISTENCE_PERSISTENCE_WRITER_HPP
#define FORM_PERSISTENCE_PERSISTENCE_WRITER_HPP

#include "ipersistence_writer.hpp"

#include "core/container_naming.hpp"
#include "core/placement.hpp"
#include "storage/istorage.hpp" // brings in form/config.hpp (tech_setting_config)

#include <map>
#include <memory>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

namespace form::detail::experimental {

  class persistence_writer : public i_persistence_writer {
  public:
    persistence_writer();
    // Test seam: inject a storage writer (e.g. a spy) instead of the default backend.
    explicit persistence_writer(std::unique_ptr<i_storage_writer> store_writer);
    ~persistence_writer() override = default;

    void configure_tech_settings(
      form::experimental::config::tech_setting_config const& tech_config_settings) override;

    void create_containers(
      std::vector<std::pair<placement, std::type_info const*>> const& containers) override;
    token register_write(placement const& plcmnt,
                         void const* data,
                         std::type_info const& type) override;
    void commit_place(placement const& plcmnt, std::string const& id) override;

  private:
    std::unique_ptr<i_storage_writer> store_writer_;
    form::experimental::config::tech_setting_config tech_settings_;
    // Product container (file, name) -> its navigation ("index") placement, resolved once when the
    // product container is created and reused on every commit. Persistence owns the index.
    std::map<std::pair<std::string, std::string>, placement> index_by_product_;
  };

} // namespace form::detail::experimental

#endif // FORM_PERSISTENCE_PERSISTENCE_WRITER_HPP
