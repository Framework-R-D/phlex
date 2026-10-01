// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_STORAGE_WRITER_HPP
#define FORM_STORAGE_STORAGE_WRITER_HPP

#include "core/placement.hpp"
#include "istorage.hpp"

#include <map>
#include <memory>
#include <string>

namespace form::detail::experimental {

  class storage_writer : public i_storage_writer {
  public:
    storage_writer() = default;
    ~storage_writer() override = default;

    using table_t = form::experimental::config::tech_setting_config::table_t;
    void create_containers(
      std::map<std::unique_ptr<placement>, std::type_info const*> const& containers,
      form::experimental::config::tech_setting_config const& settings) override;
    std::uint64_t fill_container(placement const& plcmnt,
                                 void const* data,
                                 std::type_info const& type) override;
    void commit_containers(placement const& plcmnt) override;

  private:
    std::map<std::string, std::shared_ptr<i_storage_file>> files_;
    // Keyed by placement so that technology is part of the container identity. Different
    // technologies may use the same container name within a file.
    std::map<placement, std::shared_ptr<i_storage_write_container>> write_containers_;
  };

} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_WRITER_HPP
