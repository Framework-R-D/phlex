// Copyright (C) 2025 ...

#include "storage_writer.hpp"
#include "storage_associative_write_container.hpp"
#include "storage_file.hpp"
#include "storage_write_association.hpp"

#include "util/factories.hpp"

using namespace form::detail::experimental;

namespace {
  form::experimental::config::tech_setting_config::table_t get_file_table(
    form::experimental::config::tech_setting_config const& settings,
    int technology,
    std::string const& file_name)
  {
    auto const per_tech = settings.file_settings.find(technology);
    if (per_tech == settings.file_settings.end()) {
      return {};
    }
    auto const per_file = per_tech->second.find(file_name);
    if (per_file == per_tech->second.end()) {
      return {};
    }
    return per_file->second;
  }

  form::experimental::config::tech_setting_config::table_t get_container_table(
    form::experimental::config::tech_setting_config const& settings,
    int technology,
    std::string const& container_name)
  {
    auto const per_tech = settings.container_settings.find(technology);
    if (per_tech == settings.container_settings.end()) {
      return {};
    }
    auto const per_container = per_tech->second.find(container_name);
    if (per_container == per_tech->second.end()) {
      return {};
    }
    return per_container->second;
  }
}

// Factory function implementation
namespace form::detail::experimental {
  std::unique_ptr<IStorageWriter> createStorageWriter()
  {
    return std::unique_ptr<IStorageWriter>(new StorageWriter());
  }
}

void StorageWriter::createContainers(
  std::map<std::unique_ptr<Placement>, std::type_info const*> const& containers,
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
        file =
          m_files
            .insert({plcmnt->fileName(), createFile(plcmnt->technology(), plcmnt->fileName(), 'o')})
            .first;
           for (auto const& [key, value] :
             get_file_table(settings, plcmnt->technology(), plcmnt->fileName()))
          file->second->setAttribute(key, value);
      }
      // Create and bind container to file
      auto container = createWriteContainer(plcmnt->technology(), plcmnt->containerName());
      m_write_containers.insert({key, container});
      // For associative container, create association layer
      auto associative_container =
        dynamic_pointer_cast<Storage_Associative_Write_Container>(container);
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
         get_container_table(settings, plcmnt->technology(), plcmnt->containerName()))
        container->setAttribute(key, value);
      container->setFile(file->second);
      container->setupWrite(*type);
    }
  }
  return;
}

void StorageWriter::fillContainer(Placement const& plcmnt,
                                  void const* data,
                                  std::type_info const& /* type*/)
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
