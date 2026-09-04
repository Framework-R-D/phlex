// Copyright (C) 2025 ...

#include "storage_writer.hpp"
#include "storage_associative_write_container.hpp"
#include "storage_file.hpp"
#include "storage_write_association.hpp"

#include "storage/factories.hpp"

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
  std::unique_ptr<i_storage_writer> create_storage_writer()
  {
    return std::unique_ptr<i_storage_writer>(new storage_writer());
  }
}

void storage_writer::create_containers(
  std::map<std::unique_ptr<placement>, std::type_info const*> const& containers,
  form::experimental::config::tech_setting_config const& settings)
{
  for (auto const& [plcmnt, type] : containers) {
    // Use file+container as composite key
    auto cont_key = std::make_pair(plcmnt->file_name(), plcmnt->container_name());
    auto cont = write_containers_.find(cont_key);
    if (cont == write_containers_.end()) {
      // Ensure the file exists
      auto file = files_.find(plcmnt->file_name());
      if (file == files_.end()) {
        file = files_
                 .insert({plcmnt->file_name(),
                          create_file(plcmnt->technology(), plcmnt->file_name(), 'o')})
                 .first;
        for (auto const& [key, value] :
             get_file_table(settings, plcmnt->technology(), plcmnt->file_name())) {
          file->second->set_attribute(key, value);
        }
      }
      // Create and bind container to file
      auto container = create_write_container(plcmnt->technology(), plcmnt->container_name());
      write_containers_.insert({cont_key, container});
      // For associative container, create association layer
      auto associative_container =
        dynamic_pointer_cast<storage_associative_write_container>(container);
      if (associative_container) {
        auto parent_key = std::make_pair(plcmnt->file_name(), associative_container->top_name());
        auto parent = write_containers_.find(parent_key);
        if (parent == write_containers_.end()) {
          auto parent_cont =
            create_write_association(plcmnt->technology(), associative_container->top_name());
          write_containers_.insert({parent_key, parent_cont});
          parent_cont->set_file(file->second);
          parent_cont->setup_write();
          associative_container->set_parent(parent_cont);
        } else {
          associative_container->set_parent(parent->second);
        }
      }

      for (auto const& [key, value] :
           get_container_table(settings, plcmnt->technology(), plcmnt->container_name())) {
        container->set_attribute(key, value);
      }
      container->set_file(file->second);
      container->setup_write(*type);
    }
  }
}

std::uint64_t storage_writer::fill_container(placement const& plcmnt,
                                             void const* data,
                                             std::type_info const& /* type*/)
{
  // Use file+container as composite key
  auto cont_key = std::make_pair(plcmnt.file_name(), plcmnt.container_name());
  auto cont = write_containers_.find(cont_key);
  if (cont == write_containers_.end()) {
    // FIXME: For now throw an exception here, but in future, we may have storage technology do that.
    throw std::runtime_error("storage_writer::fill_container Container doesn't exist: " +
                             plcmnt.container_name());
  }
  return cont->second->fill(data);
}

void storage_writer::commit_containers(placement const& plcmnt)
{
  auto cont_key = std::make_pair(plcmnt.file_name(), plcmnt.container_name());
  auto cont = write_containers_.find(cont_key);
  cont->second->commit();
}
