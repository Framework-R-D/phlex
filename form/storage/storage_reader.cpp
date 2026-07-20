// Copyright (C) 2025 ...

#include "storage_reader.hpp"
#include "storage_file.hpp"
#include "storage_read_container.hpp"

#include "util/factories.hpp"

#include <algorithm>
#include <cctype>

#include <map>
#include <optional>
#include <stdexcept>
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

  bool is_structured_index_id(std::string const& id)
  {
    return id.size() >= 2 && id.front() == '[' && id.back() == ']';
  }

  std::string trim_copy(std::string const& value)
  {
    auto begin = value.find_first_not_of(' ');
    if (begin == std::string::npos) {
      return {};
    }
    auto end = value.find_last_not_of(' ');
    return value.substr(begin, end - begin + 1);
  }

  std::optional<long long> parse_index_number(std::string const& value)
  {
    if (value.empty()) {
      return std::nullopt;
    }

    for (char ch : value) {
      if (!std::isdigit(static_cast<unsigned char>(ch))) {
        return std::nullopt;
      }
    }

    try {
      return std::stoll(value, nullptr, 10);
    } catch (...) {
      return std::nullopt;
    }
  }

  std::optional<std::map<std::string, long long>> normalize_structured_index(std::string const& id)
  {
    if (!is_structured_index_id(id) || id == "[]") {
      return std::nullopt;
    }

    std::string const body = id.substr(1, id.size() - 2);
    std::string token;
    std::map<std::string, long long> normalized;

    auto commit_token = [&normalized](std::string const& raw_token) -> bool {
      auto const token_trimmed = trim_copy(raw_token);
      if (token_trimmed.empty()) {
        return true;
      }

      auto const sep = token_trimmed.find_first_of(":=");
      if (sep == std::string::npos) {
        return false;
      }

      auto key = trim_copy(token_trimmed.substr(0, sep));
      auto value = trim_copy(token_trimmed.substr(sep + 1));
      if (key.empty() || value.empty()) {
        return false;
      }

      for (char& ch : key) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
      }

      auto parsed = parse_index_number(value);
      if (!parsed) {
        return false;
      }

      normalized[key] = *parsed;
      return true;
    };

    for (char ch : body) {
      if (ch == ',' || ch == ';') {
        if (!commit_token(token)) {
          return std::nullopt;
        }
        token.clear();
      } else {
        token.push_back(ch);
      }
    }

    if (!commit_token(token)) {
      return std::nullopt;
    }

    if (normalized.empty()) {
      return std::nullopt;
    }
    return normalized;
  }

  bool all_components_zero(std::map<std::string, long long> const& components)
  {
    if (components.empty()) {
      return false;
    }
    for (auto const& [key, value] : components) {
      (void)key;
      if (value != 0) {
        return false;
      }
    }
    return true;
  }

  std::optional<int> sequential_row_from_index_id(std::string const& id)
  {
    if (id == "[]") {
      return 0;
    }

    if (id.size() < 2 || id.front() != '[' || id.back() != ']') {
      return std::nullopt;
    }

    auto const body = id.substr(1, id.size() - 2);
    auto const comma = body.find(',');
    if (comma != std::string::npos) {
      return std::nullopt;
    }

    auto const colon = body.find(':');
    if (colon == std::string::npos) {
      return std::nullopt;
    }

    try {
      auto const number = std::stoi(body.substr(colon + 1));
      return number;
    } catch (...) {
      return std::nullopt;
    }
  }
}
// Factory function implementation
namespace form::detail::experimental {
  std::unique_ptr<IStorageReader> createStorageReader()
  {
    return std::unique_ptr<IStorageReader>(new StorageReader());
  }
}

int StorageReader::getIndex(Token const& token,
                            std::string const& id,
                            form::experimental::config::tech_setting_config const& settings)
{
  if (auto row = sequential_row_from_index_id(id)) {
    return *row;
  }

  if (m_indexMaps[token.containerName()].empty()) {
    auto key = std::make_pair(token.fileName(), token.containerName());
    auto cont = m_read_containers.find(key);
    if (cont == m_read_containers.end()) {
      auto file = m_files.find(token.fileName());
      if (file == m_files.end()) {
        file =
          m_files.insert({token.fileName(), createFile(token.technology(), token.fileName(), 'i')})
            .first;
        for (auto const& [key, value] :
             get_file_table(settings, token.technology(), token.fileName()))
          file->second->setAttribute(key, value);
      }
      cont = m_read_containers
               .insert({key, createReadContainer(token.technology(), token.containerName())})
               .first;
      for (auto const& [key, value] :
           get_container_table(settings, token.technology(), token.containerName()))
        cont->second->setAttribute(key, value);
      cont->second->setFile(file->second);
    }
    auto const& type = typeid(std::string);
    int entry = 0;
    void const* rawData = nullptr;
    while (cont->second->read(entry, &rawData, type)) {
      std::unique_ptr<std::string const> data(static_cast<std::string const*>(rawData));
      m_indexMaps[token.containerName()].insert(std::make_pair(*data, entry));
      entry++;
    }

    if (m_indexMaps[token.containerName()].empty()) {
      if (!is_structured_index_id(id)) {
        return 0;
      }
      throw std::runtime_error("Unable to read index data from container: " +
                               token.containerName());
    }
  }

  auto const found = m_indexMaps[token.containerName()].find(id);
  if (found != m_indexMaps[token.containerName()].end()) {
    return found->second;
  }

  auto const normalized_query = normalize_structured_index(id);
  if (normalized_query) {
    for (auto const& [existing_id, entry] : m_indexMaps[token.containerName()]) {
      auto const normalized_existing = normalize_structured_index(existing_id);
      if (normalized_existing && *normalized_existing == *normalized_query) {
        return entry;
      }
    }

    if (all_components_zero(*normalized_query)) {
      auto const empty_key = m_indexMaps[token.containerName()].find("");
      if (empty_key != m_indexMaps[token.containerName()].end()) {
        return 0;
      }

      // Compatibility fallback for backends that do not persist the first
      // structured id string in a normalized round-trippable form.
      return 0;
    }
  }

  if (!is_structured_index_id(id)) {
    return 0;
  }

  throw std::runtime_error("Index id not found: " + id + " in container: " + token.containerName());
}

void StorageReader::prime(Token const& token,
                          std::type_info const& type,
                          form::experimental::config::tech_setting_config const& settings)
{
  auto key = std::make_pair(token.fileName(), token.containerName());
  auto cont = m_read_containers.find(key);
  if (cont == m_read_containers.end()) {
    auto file = m_files.find(token.fileName());
    if (file == m_files.end()) {
      file =
        m_files.insert({token.fileName(), createFile(token.technology(), token.fileName(), 'i')})
          .first;
      for (auto const& [key, value] :
           get_file_table(settings, token.technology(), token.fileName()))
        file->second->setAttribute(key, value);
    }
    cont = m_read_containers
             .insert({key, createReadContainer(token.technology(), token.containerName())})
             .first;
    cont->second->setFile(file->second);
    for (auto const& [key, value] :
         get_container_table(settings, token.technology(), token.containerName()))
      cont->second->setAttribute(key, value);
  }
  cont->second->prime(type);
}

std::vector<std::string> StorageReader::listIndices(
  Token const& token, form::experimental::config::tech_setting_config const& settings)
{
  if (m_indexMaps[token.containerName()].empty()) {
    auto key = std::make_pair(token.fileName(), token.containerName());
    auto cont = m_read_containers.find(key);
    if (cont == m_read_containers.end()) {
      auto file = m_files.find(token.fileName());
      if (file == m_files.end()) {
        file =
          m_files.insert({token.fileName(), createFile(token.technology(), token.fileName(), 'i')})
            .first;
        for (auto const& [key, value] :
             get_file_table(settings, token.technology(), token.fileName()))
          file->second->setAttribute(key, value);
      }
      cont = m_read_containers
               .insert({key, createReadContainer(token.technology(), token.containerName())})
               .first;
      for (auto const& [key, value] :
           get_container_table(settings, token.technology(), token.containerName()))
        cont->second->setAttribute(key, value);
      cont->second->setFile(file->second);
    }

    auto const& type = typeid(std::string);
    int entry = 0;
    void const* rawData = nullptr;
    while (cont->second->read(entry, &rawData, type)) {
      std::unique_ptr<std::string const> data(static_cast<std::string const*>(rawData));
      m_indexMaps[token.containerName()].insert(std::make_pair(*data, entry));
      entry++;
    }

    if (m_indexMaps[token.containerName()].empty()) {
      throw std::runtime_error("Unable to enumerate indices from container: " +
                               token.containerName());
    }
  }

  std::vector<std::pair<int, std::string>> ordered;
  ordered.reserve(m_indexMaps[token.containerName()].size());
  for (auto const& [index_string, entry] : m_indexMaps[token.containerName()]) {
    ordered.emplace_back(entry, index_string);
  }
  std::ranges::sort(ordered,
                    [](auto const& lhs, auto const& rhs) { return lhs.first < rhs.first; });

  std::vector<std::string> result;
  result.reserve(ordered.size());
  for (auto const& [entry, index_string] : ordered) {
    (void)entry;
    result.push_back(index_string);
  }
  return result;
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
      file =
        m_files.insert({token.fileName(), createFile(token.technology(), token.fileName(), 'i')})
          .first;
      for (auto const& [key, value] :
           get_file_table(settings, token.technology(), token.fileName()))
        file->second->setAttribute(key, value);
    }
    cont = m_read_containers
             .insert({key, createReadContainer(token.technology(), token.containerName())})
             .first;
    cont->second->setFile(file->second);
    for (auto const& [key, value] :
         get_container_table(settings, token.technology(), token.containerName()))
      cont->second->setAttribute(key, value);
  }
  cont->second->read(token.id(), data, type);
  return;
}
