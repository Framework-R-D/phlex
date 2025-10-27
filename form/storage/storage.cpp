// Copyright (C) 2025 ...

#include "storage.hpp"
#include "core/placement.hpp"
#include "core/token.hpp"
#include "form/config.hpp"
#include "storage/istorage.hpp"
#include "storage_associative_container.hpp"

#include "util/factories.hpp"
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

using namespace form::detail::experimental;

// Factory function implementation
namespace form::detail::experimental {
  auto createStorage() -> std::unique_ptr<IStorage> { return std::unique_ptr<IStorage>(new Storage()); }
}

void Storage::createContainers(std::map<std::unique_ptr<Placement>, std::string> const& containers,
                               form::experimental::config::tech_setting_config const& settings)
{
  for (auto const& [plcmnt, type] : containers) {
    // Use file+container as composite key
    auto key = std::make_pair(plcmnt->fileName(), plcmnt->containerName());
    auto cont = m_containers.find(key);
    if (cont == m_containers.end()) {
      // Ensure the file exists
      auto file = m_files.find(plcmnt->fileName());
      if (file == m_files.end()) {
        m_files.insert(
          {plcmnt->fileName(), createFile(plcmnt->technology(), plcmnt->fileName(), 'o')});
        file = m_files.find(plcmnt->fileName());
        for (auto const& [key, value] :
             settings.getFileTable(plcmnt->technology(), plcmnt->fileName()))
          file->second->setAttribute(key, value);
      }
      // Create and bind container to file
      auto container = createContainer(plcmnt->technology(), plcmnt->containerName());
      m_containers.insert({key, container});
      // For associative container, create association layer
      auto associative_container = dynamic_pointer_cast<Storage_Associative_Container>(container);
      if (associative_container) {
        auto parent_key = std::make_pair(plcmnt->fileName(), associative_container->top_name());
        auto parent = m_containers.find(parent_key);
        if (parent == m_containers.end()) {
          auto parent_cont =
            createAssociation(plcmnt->technology(), associative_container->top_name());
          m_containers.insert({parent_key, parent_cont});
          parent_cont->setFile(file->second);
          parent_cont->setupWrite();
          associative_container->setParent(parent_cont);
        } else {
          associative_container->setParent(parent->second);
        }
      }

      for (auto const& [key, value] :
           settings.getContainerTable(plcmnt->technology(), plcmnt->containerName()))
        container->setAttribute(key, value);
      container->setFile(file->second);
      container->setupWrite(type);
    }
  }
  return;
}

void Storage::fillContainer(Placement const& plcmnt, void const* data, std::string const& /* type*/)
{
  // Use file+container as composite key
  auto key = std::make_pair(plcmnt.fileName(), plcmnt.containerName());
  auto cont = m_containers.find(key);
  if (cont == m_containers.end()) {
    // FIXME: For now throw an exception here, but in future, we may have storage technology do that.
    throw std::runtime_error("Storage::fillContainer Container doesn't exist: " +
                             plcmnt.containerName());
  }
  cont->second->fill(data);
  return;
}

void Storage::commitContainers(Placement const& plcmnt)
{
  auto key = std::make_pair(plcmnt.fileName(), plcmnt.containerName());
  auto cont = m_containers.find(key);
  cont->second->commit();
  return;
}

auto Storage::getIndex(Token const& token,
                      std::string const& id,
                      form::experimental::config::tech_setting_config const& settings) -> int
{
  if (m_indexMaps[token.containerName()].empty()) {
    auto key = std::make_pair(token.fileName(), token.containerName());
    auto cont = m_containers.find(key);
    if (cont == m_containers.end()) {
      auto file = m_files.find(token.fileName());
      if (file == m_files.end()) {
        m_files.insert({token.fileName(), createFile(token.technology(), token.fileName(), 'i')});
        file = m_files.find(token.fileName());
        for (auto const& [key, value] : settings.getFileTable(token.technology(), token.fileName()))
          file->second->setAttribute(key, value);
      }
      m_containers.insert({key, createContainer(token.technology(), token.containerName())});
      cont = m_containers.find(key);
      for (auto const& [key, value] :
           settings.getContainerTable(token.technology(), token.containerName()))
        cont->second->setAttribute(key, value);
      cont->second->setFile(file->second);
    }
    void const* data;
    std::string type = "std::string";
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

void Storage::readContainer(Token const& token,
                            void const** data,
                            std::string& type,
                            form::experimental::config::tech_setting_config const& settings)
{
  auto key = std::make_pair(token.fileName(), token.containerName());
  auto cont = m_containers.find(key);
  if (cont == m_containers.end()) {
    auto file = m_files.find(token.fileName());
    if (file == m_files.end()) {
      m_files.insert({token.fileName(), createFile(token.technology(), token.fileName(), 'i')});
      file = m_files.find(token.fileName());
      for (auto const& [key, value] : settings.getFileTable(token.technology(), token.fileName()))
        file->second->setAttribute(key, value);
    }
    m_containers.insert({key, createContainer(token.technology(), token.containerName())});
    cont = m_containers.find(key);
    cont->second->setFile(file->second);
    for (auto const& [key, value] :
         settings.getContainerTable(token.technology(), token.containerName()))
      cont->second->setAttribute(key, value);
  }
  cont->second->read(token.id(), data, type);
  return;
}
