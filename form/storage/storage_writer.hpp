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

  class StorageWriter : public IStorageWriter {
  public:
    StorageWriter() = default;
    ~StorageWriter() override = default;

    using table_t = form::experimental::config::tech_setting_config::table_t;
    void createContainers(
      std::map<std::unique_ptr<Placement>, std::type_info const*> const& containers,
      form::experimental::config::tech_setting_config const& settings) override;
    void fillContainer(Placement const& plcmnt,
                       void const* data,
                       std::type_info const& type) override;
    void commitContainers(Placement const& plcmnt) override;

  private:
    std::map<std::string, std::shared_ptr<IStorage_File>> m_files;
    std::unordered_map<std::pair<std::string, std::string>,
                       std::shared_ptr<IStorage_Write_Container>,
                       pair_hash>
      m_write_containers;
    std::map<std::string, std::map<std::string, int>> m_indexMaps;
  };

} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_WRITER_HPP
