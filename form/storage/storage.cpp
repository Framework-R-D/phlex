// Copyright (C) 2025 ...

#include "storage.hpp"
#include "storage_container.hpp"
#include "storage_file.hpp"

#include "util/factories.hpp"

// Factory function implementation
std::unique_ptr<IStorage> createStorage() { return std::unique_ptr<IStorage>(new Storage()); }

void Storage::fillContainer(const Placement& plcmnt, const void* data, const std::string& type)
{
  auto cont = m_containers.find(plcmnt.containerName()); //FIXME: This limits Container to File
  if (cont == m_containers.end()) {
    // Storage handles files and containers, creating new files as needed for containers
    // Containers are created on demand, using their full name, different instances of
    // associative containers (e.g. tree/branch) may share the same top-level (e.g. tree)
    auto file = m_files.find(plcmnt.fileName());
    if (file == m_files.end()) {
      m_files.insert({plcmnt.fileName(), createFile(plcmnt.technology(), plcmnt.fileName(), 'o')});
      file = m_files.find(plcmnt.fileName());
    }
    m_containers.insert(
      {plcmnt.containerName(), createContainer(plcmnt.technology(), plcmnt.containerName())});
    cont = m_containers.find(plcmnt.containerName());
    cont->second->setFile(file->second);
  }
  cont->second->fill(data, type);
  return;
}

int Storage::getIndex(const Token& token, const std::string& id)
{
  if (m_indexMaps[token.containerName()].empty()) {
    auto cont =
      m_containers.find(token.containerName()); //FIXME: This cannot be done for read/write...
    if (cont == m_containers.end()) {
      auto file = m_files.find(token.fileName());
      if (file == m_files.end()) {
        m_files.insert({token.fileName(), createFile(token.technology(), token.fileName(), 'i')});
        file = m_files.find(token.fileName());
      }
      m_containers.insert(
        {token.containerName(), createContainer(token.technology(), token.containerName())});
      cont = m_containers.find(token.containerName());
      cont->second->setFile(file->second);
    }
    const void* data;
    std::string type = "std::string";
    int entry = 1;
    while (cont->second->read(entry, &data, type)) {
      m_indexMaps[token.containerName()].insert(
        std::make_pair(*(static_cast<const std::string*>(data)), entry));
      delete static_cast<const std::string*>(
        data); //FIXME: smart pointer?  The overhead to delete an arbitrary type is not much prettier
      entry++;
    }
  }
  int entry = m_indexMaps[token.containerName()][id];
  return entry;
}

void Storage::readContainer(const Token& token, const void** data, std::string& type)
{
  auto cont =
    m_containers.find(token.containerName()); //FIXME: This cannot be done for read/write...
  if (cont == m_containers.end()) {
    auto file = m_files.find(token.fileName());
    if (file == m_files.end()) {
      m_files.insert({token.fileName(), createFile(token.technology(), token.fileName(), 'i')});
      file = m_files.find(token.fileName());
    }
    m_containers.insert(
      {token.containerName(), createContainer(token.technology(), token.containerName())});
    cont = m_containers.find(token.containerName());
    cont->second->setFile(file->second);
  }
  cont->second->read(token.id(), data, type);
  return;
}
