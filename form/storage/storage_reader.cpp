// Copyright (C) 2025 ...

#include "storage_reader.hpp"

#include "core/technology.hpp"
#include "core/token.hpp"
#include "form/config.hpp"
#include "storage/factories.hpp"
#include "storage/istorage.hpp"
#include "util/error.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
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
  auto opened = create_file(technology.major, file_name, 'i');
  for (auto const& [key, value] : get_file_table(settings, technology, file_name)) {
    opened->set_attribute(key, value);
  }
  return files_.emplace(file_name, std::move(opened)).first->second;
}

std::optional<std::vector<std::pair<std::string, std::string>>> storage_reader::read_descriptor(
  std::string const& file_name, form::technology::major technology)
{
  return create_file(technology, file_name, 'i')->read_descriptor();
}

i_storage_read_container& storage_reader::container_for(
  token const& token, form::experimental::config::tech_setting_config const& settings)
{
  auto const cont_key = std::make_pair(token.file_name(), token.container_name());
  if (auto const found = read_containers_.find(cont_key); found != read_containers_.end()) {
    return *found->second;
  }
  auto const& file = open_file(token.file_name(), token.technology(), settings);
  auto container = create_read_container(token.technology(), token.container_name());
  container->set_file(file);
  for (auto const& [key, value] :
       get_container_table(settings, token.technology(), token.container_name())) {
    container->set_attribute(key, value);
  }
  // Cache the container only after its settings have been applied successfully.
  return *read_containers_.emplace(cont_key, std::move(container)).first->second;
}

template <typename T>
std::vector<T> storage_reader::read_column(
  token const& token, form::experimental::config::tech_setting_config const& settings)
{
  auto& container = container_for(token, settings);
  std::vector<T> values;
  void const* raw = nullptr;
  for (std::uint64_t row = 0; container.read(row, &raw, typeid(T)); ++row) {
    std::unique_ptr<T const> const value{static_cast<T const*>(raw)};
    if (!value) {
      error("row " + std::to_string(row) + " of container: " + token.container_name() +
            " of file: " + token.file_name() + " was read but holds no value");
    }
    values.push_back(*value);
  }
  return values;
}

std::uint64_t storage_reader::get_index(
  token const& token,
  std::string const& id,
  form::experimental::config::tech_setting_config const& settings)
{
  auto const& index = index_map(token, settings);
  auto const found = index.find(id);
  if (found == index.end()) {
    error("Index id not found: " + id + " in container: " + token.container_name() +
          " of file: " + token.file_name());
  }
  return found->second;
}

void storage_reader::prime(token const& token,
                           std::type_info const& type,
                           form::experimental::config::tech_setting_config const& settings)
{
  container_for(token, settings).prime(type);
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

  std::map<std::string, std::uint64_t> index;
  std::uint64_t row = 0;
  for (auto& id : read_column<std::string>(token, settings)) {
    if (!index.try_emplace(std::move(id), row).second) {
      error("Index id " + id + " appears more than once in container: " + token.container_name() +
            " of file: " + token.file_name());
    }
    ++row;
  }
  if (index.empty()) {
    error("Unable to read index data from container: " + token.container_name() +
          " of file: " + token.file_name());
  }
  return index_maps_.emplace(cont_key, std::move(index)).first->second;
}

void storage_reader::read_container(token const& token,
                                    void const** data,
                                    std::type_info const& type,
                                    form::experimental::config::tech_setting_config const& settings)
{
  container_for(token, settings).read(token.id(), data, type);
}

std::vector<std::uint64_t> storage_reader::read_uint64_column(
  token const& token, form::experimental::config::tech_setting_config const& settings)
{
  return read_column<std::uint64_t>(token, settings);
}

std::vector<std::string> storage_reader::read_string_column(
  token const& token, form::experimental::config::tech_setting_config const& settings)
{
  return read_column<std::string>(token, settings);
}
