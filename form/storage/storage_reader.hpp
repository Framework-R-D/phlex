// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_STORAGE_READER_HPP
#define FORM_STORAGE_STORAGE_READER_HPP

#include "istorage.hpp"
#include "storage_utils.hpp"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility> // for std::pair

namespace form::detail::experimental {

  class storage_reader : public i_storage_reader {
  public:
    storage_reader() = default;
    ~storage_reader() override = default;

    using table_t = form::experimental::config::tech_setting_config::table_t;

    int get_index(token const& token,
                  std::string const& id,
                  form::experimental::config::tech_setting_config const& settings) override;
    void prime(token const& token,
               std::type_info const& type,
               form::experimental::config::tech_setting_config const& settings) override;
    std::vector<std::string> list_indices(
      token const& token, form::experimental::config::tech_setting_config const& settings) override;
    void read_container(token const& token,
                        void const** data,
                        std::type_info const& type,
                        form::experimental::config::tech_setting_config const& settings) override;

  private:
    std::map<std::string, std::shared_ptr<i_storage_file>> files_;
    std::unordered_map<std::pair<std::string, std::string>,
                       std::shared_ptr<i_storage_read_container>,
                       pair_hash>
      read_containers_;
    std::map<std::string, std::map<std::string, int>> index_maps_;
  };

} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_READER_HPP
