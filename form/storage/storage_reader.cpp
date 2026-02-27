// Copyright (C) 2025 ...

#include "storage_reader.hpp"
#include "storage_read_association.hpp"
#include "storage_associative_read_container.hpp"
#include "storage_file.hpp"

#include "util/factories.hpp"

using namespace form::detail::experimental;

// Factory function implementation
namespace form::detail::experimental {
  std::unique_ptr<IStorageReader> createStorageReader() { return std::unique_ptr<IStorageReader>(new StorageReader()); }
}

int StorageReader::getIndex(Token const& token,
                      std::string const& id,
                      form::experimental::config::tech_setting_config const& settings)
{
  if (m_indexMaps[token.containerName()].empty()) {
    auto key = std::make_pair(token.fileName(), token.containerName());
    auto cont = m_read_containers.find(key);
    if (cont == m_read_containers.end()) {
      auto file = m_files.find(token.fileName());
      if (file == m_files.end()) {
        m_files.insert({token.fileName(), createFile(token.technology(), token.fileName(), 'i')});
        file = m_files.find(token.fileName());
        for (auto const& [key, value] : settings.getFileTable(token.technology(), token.fileName()))
          file->second->setAttribute(key, value);
      }
      m_read_containers.insert({key, createReadContainer(token.technology(), token.containerName())});
      cont = m_read_containers.find(key);
      for (auto const& [key, value] :
           settings.getContainerTable(token.technology(), token.containerName()))
        cont->second->setAttribute(key, value);
      cont->second->setFile(file->second);
    }
    void const* data;
    auto const& type = typeid(std::string);
    int entry = 1;
    while (cont->second->read(entry, &data, type)) {
      m_indexMaps[token.containerName()].insert(
        std::make_pair(*(static_cast<std::string const*>(data)), entry));
      delete static_cast<std::string const*>(
        data); //FIXME: smart pointer?  The overhead to delete an arbitrary type is not much prettier
      entry++;
    }
  }
  int entry = m_indexMaps[token.containerName()][id];
  return entry;
}

void StorageReader::readContainer(Token const& token,
                            void const** data,
                            std::type_info const& type,
                            form::experimental::config::tech_setting_config const& settings)
{
  auto key = std::make_pair(token.fileName(), token.containerName());
  auto cont = m_read_containers.find(key);
  if (cont == m_read_containers.end()) {
    auto file = m_files.find(token.fileName());
    if (file == m_files.end()) {
      m_files.insert({token.fileName(), createFile(token.technology(), token.fileName(), 'i')});
      file = m_files.find(token.fileName());
      for (auto const& [key, value] : settings.getFileTable(token.technology(), token.fileName()))
        file->second->setAttribute(key, value);
    }
    m_read_containers.insert({key, createReadContainer(token.technology(), token.containerName())});
    cont = m_read_containers.find(key);
    cont->second->setFile(file->second);
    for (auto const& [key, value] :
         settings.getContainerTable(token.technology(), token.containerName()))
      cont->second->setAttribute(key, value);
  }
  cont->second->read(token.id(), data, type);
  return;
}
