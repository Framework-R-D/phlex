// Copyright (C) 2025 ...

#ifndef FORM_STORAGE_STORAGE_HPP
#define FORM_STORAGE_STORAGE_HPP

#include "istorage.hpp"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility> // for std::pair

namespace form::detail::experimental {

  // Hash function for std::pair
  struct pair_hash {
    template <typename T1, typename T2>
    std::size_t operator()(std::pair<T1, T2> const& p) const
    {
      std::hash<T1> h1;
      std::hash<T2> h2;
      return h1(p.first) ^ (h2(p.second) << 1);
    }
  };

  class Storage : public IStorage {
  public:
    Storage() = default;
    ~Storage() = default;

    using table_t = form::experimental::config::tech_setting_config::table_t;
    void createContainers(
      std::map<std::unique_ptr<Placement>, std::type_info const*> const& containers,
      form::experimental::config::tech_setting_config const& settings) override;
    void fillContainer(Placement const& plcmnt,
                       void const* data,
                       std::type_info const& type) override;
    void commitContainers(Placement const& plcmnt) override;

    int getIndex(Token const& token,
                 std::string const& id,
                 form::experimental::config::tech_setting_config const& settings) override;
    void readContainer(Token const& token,
                       void const** data,
                       std::type_info const& type,
                       form::experimental::config::tech_setting_config const& settings) override;

  private:
    std::map<std::string, std::shared_ptr<IStorage_File>> m_files;
    std::unordered_map<std::pair<std::string, std::string>,
                       std::shared_ptr<IStorage_Container>,
                       pair_hash>
      m_containers;
    std::map<std::string, std::map<std::string, int>> m_indexMaps;
  };

} // namespace form::detail::experimental

#endif // FORM_STORAGE_STORAGE_HPP
