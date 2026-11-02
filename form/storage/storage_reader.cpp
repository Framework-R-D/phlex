// Copyright (C) 2025 ...

#include "storage_reader.hpp"

#include "core/technology.hpp"
#include "core/token.hpp"
#include "form/config.hpp"
#include "storage/factories.hpp"
#include "storage/istorage.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>
using namespace form::detail::experimental;

namespace {
  form::experimental::config::tech_setting_config::table_t get_file_table(
    form::experimental::config::tech_setting_config const& settings,
    form::technology::id technology,
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
    form::technology::id technology,
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
  std::unique_ptr<i_storage_reader> create_storage_reader()
  {
    ensure_backend_thread_safety();
    return std::unique_ptr<i_storage_reader>(new storage_reader());
  }
}

std::shared_ptr<i_storage_file> const& storage_reader::open_file(
  std::string const& file_name,
  form::technology::id technology,
  form::experimental::config::tech_setting_config const& settings)
{
  if (auto const found = files_.find(file_name); found != files_.end()) {
    return found->second;
  }
  // Cache the file only after all file settings have been applied successfully.
  auto opened = create_file(technology, file_name, 'i');
  for (auto const& [key, value] : get_file_table(settings, technology, file_name)) {
    opened->set_attribute(key, value);
  }
  return files_.emplace(file_name, std::move(opened)).first->second;
}

std::optional<std::vector<std::pair<std::string, std::string>>> storage_reader::read_descriptor(
  std::string const& file_name,
  form::technology::id technology,
  form::experimental::config::tech_setting_config const& settings)
{
  return open_file(file_name, technology, settings)->read_descriptor();
}

std::uint64_t storage_reader::get_index(
  token const& token,
  std::string const& id,
  form::experimental::config::tech_setting_config const& settings)
{
  auto const& index = index_map(token, settings);
  auto const found = index.find(id);
  if (found == index.end()) {
    throw std::runtime_error("Index id not found: " + id + " in container: " +
                             token.container_name() + " of file: " + token.file_name());
  }
  return found->second;
}

void storage_reader::prime(token const& token,
                           std::type_info const& type,
                           form::experimental::config::tech_setting_config const& settings)
{
  auto cont_key = std::make_pair(token.file_name(), token.container_name());
  auto cont = read_containers_.find(cont_key);
  if (cont == read_containers_.end()) {
    auto const& file = open_file(token.file_name(), token.technology(), settings);
    cont = read_containers_
             .insert({cont_key, create_read_container(token.technology(), token.container_name())})
             .first;
    cont->second->set_file(file);
    for (auto const& [key, value] :
         get_container_table(settings, token.technology(), token.container_name())) {
      cont->second->set_attribute(key, value);
    }
  }
  cont->second->prime(type);
}

std::vector<std::string> storage_reader::list_indices(
  token const& token, form::experimental::config::tech_setting_config const& settings)
{
  auto const& index = index_map(token, settings);
  std::vector<std::string> result(index.size());
  for (auto const& [id, row] : index) {
    result[static_cast<std::size_t>(row)] = id;
  }
  return result;
}

std::map<std::string, std::uint64_t> const& storage_reader::index_map(
  token const& token, form::experimental::config::tech_setting_config const& settings)
{
  auto const cont_key = std::make_pair(token.file_name(), token.container_name());
  if (auto const built = index_maps_.find(cont_key); built != index_maps_.end()) {
    return built->second;
  }

  auto cont = read_containers_.find(cont_key);
  if (cont == read_containers_.end()) {
    auto const& file = open_file(token.file_name(), token.technology(), settings);
    cont = read_containers_
             .insert({cont_key, create_read_container(token.technology(), token.container_name())})
             .first;
    for (auto const& [key, value] :
         get_container_table(settings, token.technology(), token.container_name())) {
      cont->second->set_attribute(key, value);
    }
    cont->second->set_file(file);
  }

  std::map<std::string, std::uint64_t> index;
  auto const& type = typeid(std::string);
  std::uint64_t row = 0;
  void const* raw_data = nullptr;
  while (cont->second->read(row, &raw_data, type)) {
    std::unique_ptr<std::string const> const id(static_cast<std::string const*>(raw_data));
    if (!index.emplace(*id, row).second) {
      throw std::runtime_error("Index id " + *id + " appears more than once in container: " +
                               token.container_name() + " of file: " + token.file_name());
    }
    ++row;
  }
  if (index.empty()) {
    throw std::runtime_error("Unable to read index data from container: " + token.container_name() +
                             " of file: " + token.file_name());
  }
  return index_maps_.emplace(cont_key, std::move(index)).first->second;
}

void storage_reader::read_container(token const& token,
                                    void const** data,
                                    std::type_info const& type,
                                    form::experimental::config::tech_setting_config const& settings)
{
  auto cont_key = std::make_pair(token.file_name(), token.container_name());
  auto cont = read_containers_.find(cont_key);
  if (cont == read_containers_.end()) {
    auto const& file = open_file(token.file_name(), token.technology(), settings);
    cont = read_containers_
             .insert({cont_key, create_read_container(token.technology(), token.container_name())})
             .first;
    cont->second->set_file(file);
    for (auto const& [key, value] :
         get_container_table(settings, token.technology(), token.container_name())) {
      cont->second->set_attribute(key, value);
    }
  }
  cont->second->read(token.id(), data, type);
}
