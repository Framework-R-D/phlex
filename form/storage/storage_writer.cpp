// Copyright (C) 2025 ...

#include "storage_writer.hpp"
#include "storage_write_association.hpp"
#include "storage_associative_write_container.hpp"
#include "storage_file.hpp"

#include "util/factories.hpp"

using namespace form::detail::experimental;

// Factory function implementation
namespace form::detail::experimental {
  std::unique_ptr<IStorageWriter> createStorageWriter() { return std::unique_ptr<IStorageWriter>(new StorageWriter()); }
}

void StorageWriter::createContainers(std::map<std::unique_ptr<Placement>, std::type_info const*> const& containers,
                               form::experimental::config::tech_setting_config const& settings)
{
  for (auto const& [plcmnt, type] : containers) {
    // Use file+container as composite key
    auto key = std::make_pair(plcmnt->fileName(), plcmnt->containerName());
    auto cont = m_write_containers.find(key);
    if (cont == m_write_containers.end()) {
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
      auto container = createWriteContainer(plcmnt->technology(), plcmnt->containerName());
      m_write_containers.insert({key, container});
      // For associative container, create association layer
      auto associative_container = dynamic_pointer_cast<Storage_Associative_Write_Container>(container);
      if (associative_container) {
        auto parent_key = std::make_pair(plcmnt->fileName(), associative_container->top_name());
        auto parent = m_write_containers.find(parent_key);
        if (parent == m_write_containers.end()) {
          auto parent_cont =
            createWriteAssociation(plcmnt->technology(), associative_container->top_name());
          m_write_containers.insert({parent_key, parent_cont});
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
      container->setupWrite(*type);
    }
  }
  return;
}

void StorageWriter::fillContainer(Placement const& plcmnt, void const* data, std::type_info const& /* type*/)
{
  // Use file+container as composite key
  auto key = std::make_pair(plcmnt.fileName(), plcmnt.containerName());
  auto cont = m_write_containers.find(key);
  if (cont == m_write_containers.end()) {
    // FIXME: For now throw an exception here, but in future, we may have storage technology do that.
    throw std::runtime_error("StorageWriter::fillContainer Container doesn't exist: " +
                             plcmnt.containerName());
  }
  cont->second->fill(data);
  return;
}

void StorageWriter::commitContainers(Placement const& plcmnt)
{
  auto key = std::make_pair(plcmnt.fileName(), plcmnt.containerName());
  auto cont = m_write_containers.find(key);
  cont->second->commit();
  return;
}
