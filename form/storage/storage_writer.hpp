// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_STORAGE_WRITER_HPP
#define FORM_STORAGE_STORAGE_WRITER_HPP

#include "istorage.hpp"
#include "storage_utils.hpp"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility> // for std::pair

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
    std::unordered_map<std::pair<std::string, std::string>,
                       std::shared_ptr<i_storage_write_container>,
                       pair_hash>
      write_containers_;
    std::map<std::string, std::map<std::string, int>> index_maps_;
  };

} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_WRITER_HPP
