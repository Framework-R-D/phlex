// Copyright (C) 2025 ...

#ifndef __STORAGE_H__
#define __STORAGE_H__

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
    std::size_t operator()(const std::pair<T1, T2>& p) const
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

    void createContainers(const std::map<std::unique_ptr<Placement>, std::string>& containers);
    void fillContainer(const Placement& plcmnt, const void* data, const std::string& type);
    void commitContainers(const Placement& plcmnt);

    int getIndex(const Token& token, const std::string& id);
    void readContainer(const Token& token, const void** data, std::string& type);

  private:
    std::map<std::string, std::shared_ptr<IStorage_File>> m_files;
    std::unordered_map<std::pair<std::string, std::string>,
                       std::shared_ptr<IStorage_Container>,
                       pair_hash>
      m_containers;
    std::map<std::string, std::map<std::string, int>> m_indexMaps;
  };

} // namespace form::detail::experimental

#endif
