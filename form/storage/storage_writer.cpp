// Copyright (C) 2025 ...

#include "storage_writer.hpp"
#include "storage_associative_write_container.hpp"
#include "storage_file.hpp"
#include "storage_write_association.hpp"

#include "form/technology.hpp"
#include "util/factories.hpp"

#include <cctype>
#include <charconv>
#include <cstdint>
#include <iomanip>
#include <random>
#include <set>
#include <sstream>

#ifdef USE_ROOT_STORAGE
#include "TFile.h"
#include "TObjString.h"
#include "TTree.h"
#include "root_storage/root_tfile.hpp"
#include <TUUID.h>
#endif

using namespace form::detail::experimental;

namespace {
  form::experimental::config::tech_setting_config::table_t lookup_file_table(
    form::experimental::config::tech_setting_config const& settings,
    int technology,
    std::string const& file_name)
  {
    auto const per_tech_it = settings.file_settings.find(technology);
    if (per_tech_it == settings.file_settings.end()) {
      return {};
    }
    auto const file_it = per_tech_it->second.find(file_name);
    if (file_it == per_tech_it->second.end()) {
      return {};
    }
    return file_it->second;
  }

  form::experimental::config::tech_setting_config::table_t lookup_container_table(
    form::experimental::config::tech_setting_config const& settings,
    int technology,
    std::string const& container_name)
  {
    auto const per_tech_it = settings.container_settings.find(technology);
    if (per_tech_it == settings.container_settings.end()) {
      return {};
    }
    auto const container_it = per_tech_it->second.find(container_name);
    if (container_it == per_tech_it->second.end()) {
      return {};
    }
    return container_it->second;
  }

  std::string trim_copy(std::string_view input)
  {
    auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
    while (!input.empty() && is_space(static_cast<unsigned char>(input.front()))) {
      input.remove_prefix(1);
    }
    while (!input.empty() && is_space(static_cast<unsigned char>(input.back()))) {
      input.remove_suffix(1);
    }
    return std::string{input};
  }

  bool parse_uint64(std::string const& value_text, std::uint64_t& value)
  {
    if (value_text.empty()) {
      return false;
    }

    int base = 10;
    if (value_text.size() > 2 && value_text[0] == '0' &&
        (value_text[1] == 'x' || value_text[1] == 'X')) {
      base = 16;
    } else {
      bool has_hex_letter = false;
      for (char ch : value_text) {
        if ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) {
          has_hex_letter = true;
          break;
        }
      }
      if (has_hex_letter) {
        base = 16;
      }
    }

    auto const* begin = value_text.data();
    auto const* end = value_text.data() + value_text.size();
    auto [ptr, ec] = std::from_chars(begin, end, value, base);
    return ec == std::errc{} && ptr == end;
  }

  bool parse_layer_index_string(std::string const& index_text,
                                std::vector<std::string>& schema,
                                std::vector<std::uint64_t>& values)
  {
    schema.clear();
    values.clear();

    auto content = trim_copy(index_text);
    if (content.size() >= 2 && content.front() == '[' && content.back() == ']') {
      content = content.substr(1, content.size() - 2);
    }

    std::size_t start = 0;
    while (start < content.size()) {
      auto end = content.find_first_of(",;", start);
      auto token = trim_copy(
        content.substr(start, end == std::string::npos ? std::string::npos : end - start));
      if (!token.empty()) {
        auto sep_pos = token.find(':');
        if (sep_pos == std::string::npos) {
          sep_pos = token.find('=');
        }
        if (sep_pos == std::string::npos) {
          return false;
        }

        auto layer_name = trim_copy(token.substr(0, sep_pos));
        auto value_text = trim_copy(token.substr(sep_pos + 1));
        if (layer_name.empty()) {
          return false;
        }

        std::uint64_t value = 0;
        if (!parse_uint64(value_text, value)) {
          return false;
        }

        schema.push_back(std::move(layer_name));
        values.push_back(value);
      }

      if (end == std::string::npos) {
        break;
      }
      start = end + 1;
    }

    return !schema.empty() && schema.size() == values.size();
  }

  bool is_index_container(std::string const& container_name)
  {
    return container_name.size() >= 6 &&
           container_name.compare(container_name.size() - 6, 6, "/index") == 0;
  }

  std::pair<std::string, std::string> split_creator_and_product(std::string const& container_name)
  {
    std::size_t const slash = container_name.find('/');
    if (slash == std::string::npos) {
      return {std::string{}, container_name};
    }

    std::string creator = container_name.substr(0, slash);
    std::string product = container_name.substr(slash + 1);
    return {std::move(creator), std::move(product)};
  }

  bool setting_key_is_process_name(std::string const& key)
  {
    std::string lowered;
    lowered.reserve(key.size());
    for (char c : key) {
      lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return lowered == "processname" || lowered == "process_name";
  }

  std::string resolve_process_name(form::experimental::config::tech_setting_config const& settings,
                                   std::string const& file_name)
  {
    for (auto const& [technology, by_file] : settings.file_settings) {
      (void)technology;
      auto const file_it = by_file.find(file_name);
      if (file_it == by_file.end()) {
        continue;
      }
      for (auto const& [key, value] : file_it->second) {
        if (setting_key_is_process_name(key) && !value.empty()) {
          return value;
        }
      }
    }
    return std::string();
  }

  std::string build_product_id(std::string const& product_name,
                               std::string const& producer,
                               std::string const& process_name)
  {
    std::string product_id;
    product_id.reserve(product_name.size() + producer.size() + process_name.size() + 2);
    product_id += product_name;
    product_id += '|';
    product_id += producer;
    product_id += '|';
    product_id += process_name;
    return product_id;
  }

  std::string serialize_layer_schema(std::vector<std::string> const& schema)
  {
    std::ostringstream oss;
    oss << '[';
    for (std::size_t i = 0; i < schema.size(); ++i) {
      if (i > 0) {
        oss << ',';
      }
      oss << '"' << schema[i] << '"';
    }
    oss << ']';
    return oss.str();
  }

  std::string schema_key_for_match(std::vector<std::string> const& schema)
  {
    std::ostringstream oss;
    for (std::size_t i = 0; i < schema.size(); ++i) {
      if (i > 0) {
        oss << ',';
      }
      oss << schema[i];
    }
    return oss.str();
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
             lookup_file_table(settings, plcmnt->technology(), plcmnt->fileName()))
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
           lookup_container_table(settings, plcmnt->technology(), plcmnt->containerName()))
        container->setAttribute(key, value);
      container->setFile(file->second);
      container->setupWrite(*type);
    }
  }
  return;
}

void StorageWriter::fillContainer(Placement const& plcmnt,
                                  void const* data,
                                  std::type_info const& /* type*/,
                                  std::string const& product_name)
{
  // Use file+container as composite key
  auto key = std::make_pair(plcmnt.fileName(), plcmnt.containerName());
  auto cont = m_write_containers.find(key);
  if (cont == m_write_containers.end()) {
    // FIXME: For now throw an exception here, but in future, we may have storage technology do that.
    throw std::runtime_error("StorageWriter::fillContainer Container doesn't exist: " +
                             plcmnt.containerName());
  }

  auto const [creator_name, product_from_container] =
    split_creator_and_product(plcmnt.containerName());
  (void)product_from_container;

  if (!is_index_container(plcmnt.containerName())) {
    if (!creator_name.empty()) {
      std::string const logical_product_name = !product_name.empty() ? product_name : creator_name;
      m_productsByProducer[plcmnt.fileName()][creator_name].insert(logical_product_name);
      auto& pending_products = m_pendingProductsByProducer[plcmnt.fileName()][creator_name];
      if (std::find(pending_products.begin(), pending_products.end(), logical_product_name) ==
          pending_products.end()) {
        pending_products.push_back(logical_product_name);
      }
    }
  }

  std::uint64_t payload_row = 0;
  if (is_index_container(plcmnt.containerName())) {
    // Keep payload row 0-based so it maps directly to entry/row ids.
    payload_row = cont->second->getEntryCount();
  }

  if (is_index_container(plcmnt.containerName()) && data != nullptr) {
    auto const* segment_id = static_cast<std::string const*>(data);
    std::vector<std::string> schema;
    std::vector<std::uint64_t> values;
    if (parse_layer_index_string(*segment_id, schema, values)) {
      auto& pending_by_producer = m_pendingProductsByProducer[plcmnt.fileName()];
      auto pending_it = pending_by_producer.find(creator_name);

      std::vector<std::string> products_for_index;
      if (pending_it != pending_by_producer.end()) {
        products_for_index = std::move(pending_it->second);
        pending_by_producer.erase(pending_it);
      }

      if (products_for_index.empty() && !creator_name.empty()) {
        auto const all_by_producer = m_productsByProducer.find(plcmnt.fileName());
        if (all_by_producer != m_productsByProducer.end()) {
          auto const all_products_it = all_by_producer->second.find(creator_name);
          if (all_products_it != all_by_producer->second.end()) {
            products_for_index.assign(all_products_it->second.begin(),
                                      all_products_it->second.end());
          }
        }
      }

      if (products_for_index.empty()) {
        m_indexLayerSchemas[plcmnt.fileName()].push_back(std::move(schema));
        m_indexLayerValues[plcmnt.fileName()].push_back(std::move(values));
        m_indexProductNames[plcmnt.fileName()].push_back(std::string());
        m_indexProducers[plcmnt.fileName()].push_back(std::string());
        m_indexContainerNames[plcmnt.fileName()].push_back(std::string());
        m_indexPayloadRows[plcmnt.fileName()].push_back(payload_row);
      } else {
        for (auto const& pending_product : products_for_index) {
          m_indexLayerSchemas[plcmnt.fileName()].push_back(schema);
          m_indexLayerValues[plcmnt.fileName()].push_back(values);
          m_indexProductNames[plcmnt.fileName()].push_back(pending_product);
          m_indexProducers[plcmnt.fileName()].push_back(creator_name);
          m_indexContainerNames[plcmnt.fileName()].push_back(creator_name);
          m_indexPayloadRows[plcmnt.fileName()].push_back(payload_row);
        }
      }
    }
  }

  (void)product_name;

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
  TUUID uuid;
  return uuid.AsString();
}

void StorageWriter::finalize(form::experimental::config::tech_setting_config const& settings)
{
  // Write FileCatalog metadata tree for each file
#ifdef USE_ROOT_STORAGE
  for (auto const& [fileName, file] : m_files) {
    // Try to downcast to ROOT file implementation
    ROOT_TFileImp* root_file = dynamic_cast<ROOT_TFileImp*>(file.get());
    if (root_file == nullptr) {
      continue;
    }

    auto tfile = root_file->getTFile();
    if (!tfile) {
      continue;
    }

    // Preserve any existing FileCatalog metadata if reopening the file.
    TObject* existing_catalog_obj = tfile->Get("FileCatalog");
    TTree* existing_catalog = dynamic_cast<TTree*>(existing_catalog_obj);
    if (existing_catalog != nullptr) {
      // Existing FileCatalog already contains the persisted FileUUID.
      continue;
    }

    // Create FileCatalog tree for new files.
    auto catalog = std::make_unique<TTree>("FileCatalog", "File-level metadata catalog");
    catalog->SetDirectory(nullptr);

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
    tfile->WriteTObject(catalog.get());

    // Create ProductRegistry tree listing logical product names from user/framework config.
    // Use explicit logical product names instead of deriving from container structure.
    std::string const user_provided_process_name = resolve_process_name(settings, fileName);

    auto products_it = m_productsByProducer.find(fileName);
    if (products_it != m_productsByProducer.end() && !products_it->second.empty()) {
      auto const& products_by_producer = products_it->second;
      auto registry = std::make_unique<TTree>("ProductRegistry", "Product-level metadata catalog");
      registry->SetDirectory(nullptr);

      std::string productName;
      std::string processName;
      std::string producer;
      std::string productID;

      registry->Branch("ProductName", &productName);
      registry->Branch("ProcessName", &processName);
      registry->Branch("Producer", &producer);
      registry->Branch("ProductID", &productID);

      for (auto const& [creator, product_names] : products_by_producer) {
        for (auto const& nm : product_names) {
          productName = nm;
          processName = user_provided_process_name;
          producer = creator;
          productID = build_product_id(productName, producer, processName);
          registry->Fill();
        }
      }
      tfile->WriteTObject(registry.get());
    }

    auto schema_it = m_indexLayerSchemas.find(fileName);
    auto values_it = m_indexLayerValues.find(fileName);
    auto idx_products_it = m_indexProductNames.find(fileName);
    auto idx_producers_it = m_indexProducers.find(fileName);
    auto containers_it = m_indexContainerNames.find(fileName);
    auto payload_rows_it = m_indexPayloadRows.find(fileName);
    if (schema_it != m_indexLayerSchemas.end() && values_it != m_indexLayerValues.end() &&
        idx_products_it != m_indexProductNames.end() &&
        idx_producers_it != m_indexProducers.end() &&
        containers_it != m_indexContainerNames.end() &&
        payload_rows_it != m_indexPayloadRows.end()) {
      auto const& schemas = schema_it->second;
      auto const& values = values_it->second;
      auto const& product_names = idx_products_it->second;
      auto const& producers = idx_producers_it->second;
      auto const& container_names = containers_it->second;
      auto const& payload_rows = payload_rows_it->second;
      if (!schemas.empty() && schemas.size() == values.size() &&
          schemas.size() == product_names.size() && schemas.size() == producers.size() &&
          schemas.size() == container_names.size() && schemas.size() == payload_rows.size()) {
        std::string canonical_schema_key;
        std::size_t canonical_schema_size = 0;
        std::size_t canonical_schema_count = 0;
        std::map<std::string, std::pair<std::size_t, std::size_t>> schema_stats;
        for (auto const& schema : schemas) {
          std::string schema_key = schema_key_for_match(schema);
          auto& [count, size] = schema_stats[schema_key];
          ++count;
          size = schema.size();
          if (count > canonical_schema_count ||
              (count == canonical_schema_count && size > canonical_schema_size)) {
            canonical_schema_count = count;
            canonical_schema_size = size;
            canonical_schema_key = schema_key;
          }
        }

        if (canonical_schema_key.empty()) {
          continue;
        }

        auto index_registry =
          std::make_unique<TTree>("IndexRegistry", "Layer index metadata catalog");
        index_registry->SetDirectory(nullptr);

        std::vector<std::string> canonical_schema;
        {
          std::size_t start = 0;
          while (start <= canonical_schema_key.size()) {
            std::size_t end = canonical_schema_key.find(',', start);
            std::string token = canonical_schema_key.substr(
              start, end == std::string::npos ? std::string::npos : end - start);
            if (!token.empty()) {
              canonical_schema.push_back(token);
            }
            if (end == std::string::npos) {
              break;
            }
            start = end + 1;
          }
        }

        std::vector<std::uint64_t> layer_values_by_branch(canonical_schema.size(), 0);
        for (std::size_t i = 0; i < canonical_schema.size(); ++i) {
          std::string const leaf_list = canonical_schema[i] + "/l";
          index_registry->Branch(
            canonical_schema[i].c_str(), &layer_values_by_branch[i], leaf_list.c_str());
        }
        std::string index_product_id;
        index_registry->Branch("ProductID", &index_product_id);
        std::string index_container_name;
        index_registry->Branch("ContainerName", &index_container_name);
        std::uint64_t index_payload_row = 0;
        index_registry->Branch("PayloadRow", &index_payload_row, "PayloadRow/l");

        // Keep schema in tree header metadata as ["layer1","layer2",...].
        std::string schema_header = serialize_layer_schema(canonical_schema);
        TList* userInfo = index_registry->GetUserInfo();
        if (userInfo) {
          userInfo->SetOwner(kTRUE);
          // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
          userInfo->Add(new TObjString(schema_header.c_str()));
        }

        for (std::size_t i = 0; i < schemas.size(); ++i) {
          if (schema_key_for_match(schemas[i]) != canonical_schema_key) {
            continue;
          }
          for (std::size_t layer_idx = 0; layer_idx < canonical_schema.size(); ++layer_idx) {
            layer_values_by_branch[layer_idx] = values[i][layer_idx];
          }
          index_product_id =
            build_product_id(product_names[i], producers[i], user_provided_process_name);
          index_container_name = container_names[i];
          index_payload_row = payload_rows[i];
          index_registry->Fill();
        }

        tfile->WriteTObject(index_registry.get());
      }
    }
  }
#endif
  return;
}
