// Copyright (C) 2025 ...

#include "storage_writer.hpp"
#include "storage_associative_write_container.hpp"
#include "storage_file.hpp"
#include "storage_write_association.hpp"

#include "util/factories.hpp"
#include "form/technology.hpp"

#include <iomanip>
#include <random>
#include <sstream>
#include <set>

#ifdef USE_ROOT_STORAGE
#include "root_storage/root_tfile.hpp"
#include "TFile.h"
#include "TTree.h"
#endif

using namespace form::detail::experimental;

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
             settings.getFileTable(plcmnt->technology(), plcmnt->fileName()))
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
           settings.getContainerTable(plcmnt->technology(), plcmnt->containerName()))
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

static std::string generateUUID()
{
  std::random_device rd;
  std::mt19937_64 generator(rd());
  std::uniform_int_distribution<uint32_t> distribution(0, 0xFFFFFFFFu);

  uint32_t w0 = distribution(generator);
  uint32_t w1 = distribution(generator);
  uint32_t w2 = distribution(generator);
  uint32_t w3 = distribution(generator);

  // RFC 4122 variant 4 UUID: set the version and variant bits.
  uint16_t time_hi_and_version = static_cast<uint16_t>((w1 >> 16) & 0x0FFFu);
  time_hi_and_version |= 0x4000u;
  uint16_t clock_seq_hi_and_reserved = static_cast<uint16_t>((w2 >> 16) & 0x3FFFu);
  clock_seq_hi_and_reserved |= 0x8000u;

  std::ostringstream oss;
  oss << std::hex << std::nouppercase << std::setfill('0');
  oss << std::setw(8) << w0 << '-';
  oss << std::setw(4) << static_cast<uint16_t>(w1 >> 16) << '-';
  oss << std::setw(4) << time_hi_and_version << '-';
  oss << std::setw(4) << clock_seq_hi_and_reserved << '-';
  oss << std::setw(4) << static_cast<uint16_t>(w2 & 0xFFFFu);
  oss << std::setw(8) << w3;

  return oss.str();
}

void StorageWriter::finalize(form::experimental::config::tech_setting_config const& /* settings */)
{
  // Write FileCatalog metadata tree for each file
#ifdef USE_ROOT_STORAGE
  for (auto const& [fileName, file] : m_files) {
    // Try to downcast to ROOT file implementation
    ROOT_TFileImp* root_file = dynamic_cast<ROOT_TFileImp*>(file.get());
    if (root_file != nullptr) {
      auto tfile = root_file->getTFile();
      if (tfile) {
        // Preserve any existing FileCatalog metadata if reopening the file.
        TObject* existing_catalog_obj = tfile->Get("FileCatalog");
        TTree* existing_catalog = static_cast<TTree*>(existing_catalog_obj);
        if (existing_catalog != nullptr) {
          // Existing FileCatalog already contains the persisted FileUUID.
          continue;
        }

        // Create FileCatalog tree for new files.
        TTree* catalog = new TTree("FileCatalog", "File-level metadata catalog");

        // Determine FileFormatVersion based on technology
        // We need to get technology from somewhere - for now, infer from existing containers
        int fileFormatVersion = 1; // Default: ROOT TTree

        // Check if any container in this file is ROOT_RNTUPLE
        for (auto const& [key, container] : m_write_containers) {
          if (key.first == fileName) {
            // We could check technology here, but for now assume ROOT_TTREE (version 1)
            // In future, this could be enhanced to detect ROOT_RNTUPLE (version 2)
            break;
          }
        }

        std::string fileUUID = generateUUID();
        catalog->Branch("FileUUID", &fileUUID);
        // Add FileFormatVersion branch
        catalog->Branch("FileFormatVersion", &fileFormatVersion, "FileFormatVersion/I");

        // Fill the tree with one entry
        catalog->Fill();

        // Write to file
        tfile->WriteTObject(catalog);
        delete catalog;

        // Create ProductRegistry tree listing payload data product trees for this file.
        // The product registry should describe top-level payload TTrees, not individual
        // branch or container labels within those trees.
        std::set<std::string> product_names;
        for (auto const& [key, container] : m_write_containers) {
          if (key.first != fileName)
            continue;
          std::string const& container_name = key.second;
          if (container_name.size() >= 6 && container_name.compare(container_name.size() - 6, 6, "/index") == 0)
            continue;
          if (container_name.find('/') != std::string::npos)
            continue;
          product_names.insert(container_name);
        }
        if (!product_names.empty()) {
          TTree* registry = new TTree("ProductRegistry", "Product-level metadata catalog");
          std::string productName;
          std::string processName;
          std::string producer;

          registry->Branch("ProductName", &productName);
          registry->Branch("ProcessName", &processName);
          registry->Branch("Producer", &producer);

          for (auto const& nm : product_names) {
            productName = nm;
            processName = std::string(); // default empty; user can annotate later
            producer = nm; // default producer is the top-level creator/tree name
            registry->Fill();
          }
          tfile->WriteTObject(registry);
          delete registry;
        }
      }
    }
  }
#endif
  return;
}
