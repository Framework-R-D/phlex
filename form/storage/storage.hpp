// Copyright (C) 2025 ...

#ifndef __STORAGE_H__
#define __STORAGE_H__

#include "istorage.hpp"

#include <map>
#include <memory>
#include <string>

class Storage : public IStorage {
public:
  Storage() = default;
  ~Storage() = default;

  void fillContainer(const Placement& plcmnt, const void* data, const std::string& type);

  int getIndex(const Token& token, const std::string& id);
  void readContainer(const Token& token, const void** data, std::string& type);

private:
  std::map<std::string, std::shared_ptr<IStorage_File>> m_files;
  std::map<std::string, std::shared_ptr<IStorage_Container>> m_containers;

  std::map<std::string, std::map<std::string, int>> m_indexMaps;
};

#endif
