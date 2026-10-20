// Copyright (C) 2025 ...

#include "storage_reader.hpp"

#include "core/technology.hpp"
#include "core/token.hpp"
#include "form/config.hpp"
#include "storage/factories.hpp"
#include "storage/istorage.hpp"

#include <cstddef>
#include <map>
#include <memory>
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
    return std::unique_ptr<i_storage_reader>(new storage_reader());
  }
}

int storage_reader::get_index(token const& token,
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
    auto file = files_.find(token.file_name());
    if (file == files_.end()) {
      file =
        files_.insert({token.file_name(), create_file(token.technology(), token.file_name(), 'i')})
          .first;
      for (auto const& [key, value] :
           get_file_table(settings, token.technology(), token.file_name())) {
        file->second->set_attribute(key, value);
      }
    }
    cont = read_containers_
             .insert({cont_key, create_read_container(token.technology(), token.container_name())})
             .first;
    cont->second->set_file(file->second);
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

std::map<std::string, int> const& storage_reader::index_map(
  token const& token, form::experimental::config::tech_setting_config const& settings)
{
  auto const cont_key = std::make_pair(token.file_name(), token.container_name());
  if (auto const built = index_maps_.find(cont_key); built != index_maps_.end()) {
    return built->second;
  }

  auto cont = read_containers_.find(cont_key);
  if (cont == read_containers_.end()) {
    auto file = files_.find(token.file_name());
    if (file == files_.end()) {
      file =
        files_.insert({token.file_name(), create_file(token.technology(), token.file_name(), 'i')})
          .first;
      for (auto const& [key, value] :
           get_file_table(settings, token.technology(), token.file_name())) {
        file->second->set_attribute(key, value);
      }
    }
    cont = read_containers_
             .insert({cont_key, create_read_container(token.technology(), token.container_name())})
             .first;
    for (auto const& [key, value] :
         get_container_table(settings, token.technology(), token.container_name())) {
      cont->second->set_attribute(key, value);
    }
    cont->second->set_file(file->second);
  }

  std::map<std::string, int> index;
  auto const& type = typeid(std::string);
  int row = 0;
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
    auto file = files_.find(token.file_name());
    if (file == files_.end()) {
      file =
        files_.insert({token.file_name(), create_file(token.technology(), token.file_name(), 'i')})
          .first;
      for (auto const& [key, value] :
           get_file_table(settings, token.technology(), token.file_name())) {
        file->second->set_attribute(key, value);
      }
    }
    cont = read_containers_
             .insert({cont_key, create_read_container(token.technology(), token.container_name())})
             .first;
    cont->second->set_file(file->second);
    for (auto const& [key, value] :
         get_container_table(settings, token.technology(), token.container_name())) {
      cont->second->set_attribute(key, value);
    }
  }
  // TODO: token::id() is a 64-bit row; the read container interface still takes an int entry. Narrow explicitly here (exact for all realistic row counts). Widening the read path to 64-bit is a follow-up PR.
  cont->second->read(static_cast<int>(token.id()), data, type);
}
