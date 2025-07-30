// Copyright (C) 2025 ...

#include "storage.hpp"
#include "storage_association.hpp"
#include "storage_associative_container.hpp"
#include "storage_file.hpp"

#include "util/factories.hpp"

using namespace form::detail::experimental;

// Factory function implementation
namespace form::detail::experimental {
  std::unique_ptr<IStorage> createStorage() { return std::unique_ptr<IStorage>(new Storage()); }
}

void Storage::createContainers(const std::map<std::unique_ptr<Placement>, std::string>& containers)
{
  for (const auto& [plcmnt, type] : containers) {
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
      container->setFile(file->second);
      container->setupWrite(type);
    }
  }
  return;
}

void Storage::fillContainer(const Placement& plcmnt, const void* data, const std::string& /* type*/)
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

void Storage::commitContainers(const Placement& plcmnt)
{
  auto key = std::make_pair(plcmnt.fileName(), plcmnt.containerName());
  auto cont = m_containers.find(key);
  cont->second->commit();
  return;
}

int Storage::getIndex(const Token& token, const std::string& id)
{
  if (m_indexMaps[token.containerName()].empty()) {
    auto key = std::make_pair(token.fileName(), token.containerName());
    auto cont = m_containers.find(key);
    if (cont == m_containers.end()) {
      auto file = m_files.find(token.fileName());
      if (file == m_files.end()) {
        m_files.insert({token.fileName(), createFile(token.technology(), token.fileName(), 'i')});
        file = m_files.find(token.fileName());
      }
      m_containers.insert({key, createContainer(token.technology(), token.containerName())});
      cont = m_containers.find(key);
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
  auto key = std::make_pair(token.fileName(), token.containerName());
  auto cont = m_containers.find(key);
  if (cont == m_containers.end()) {
    auto file = m_files.find(token.fileName());
    if (file == m_files.end()) {
      m_files.insert({token.fileName(), createFile(token.technology(), token.fileName(), 'i')});
      file = m_files.find(token.fileName());
    }
    m_containers.insert({key, createContainer(token.technology(), token.containerName())});
    cont = m_containers.find(key);
    cont->second->setFile(file->second);
  }
  cont->second->read(token.id(), data, type);
  return;
}
