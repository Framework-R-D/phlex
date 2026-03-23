// Copyright (C) 2025 ...

#include "persistence_writer.hpp"
#include "persistence_utils.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>

using namespace form::detail::experimental;

namespace form::detail::experimental {
  std::unique_ptr<IPersistenceWriter> createPersistenceWriter()
  {
    return std::make_unique<PersistenceWriter>();
  }
}

PersistenceWriter::PersistenceWriter() :
  m_store(createStorageWriter()),
  m_output_items(),
  m_tech_settings() // constructor takes form config
{
}

void PersistenceWriter::configureTechSettings(
  form::experimental::config::tech_setting_config const& tech_config_settings)
{
  m_tech_settings = tech_config_settings;
}

void PersistenceWriter::configureOutputItems(
  form::experimental::config::output_item_config const& output_items)
{
  m_output_items = output_items;
}

void PersistenceWriter::createContainers(
  std::string const& creator, std::map<std::string, std::type_info const*> const& products)
{
  std::map<std::unique_ptr<Placement>, std::type_info const*> containers;
  for (auto const& [label, type] : products) {
    containers.insert(std::make_pair(getPlacement(creator, label), type));
  }
  containers.insert(std::make_pair(getPlacement(creator, "index"), &typeid(std::string)));
  m_store->createContainers(containers, m_tech_settings);
  return;
}

void PersistenceWriter::registerWrite(std::string const& creator,
                                      std::string const& label,
                                      void const* data,
                                      std::type_info const& type)
{
  std::unique_ptr<Placement> plcmnt = getPlacement(creator, label);
  m_store->fillContainer(*plcmnt, data, type);
  return;
}

void PersistenceWriter::commitOutput(std::string const& creator, std::string const& id)
{
  std::unique_ptr<Placement> plcmnt = getPlacement(creator, "index");
  m_store->fillContainer(*plcmnt, &id, typeid(std::string));
  m_store->commitContainers(*plcmnt);
  return;
}

std::unique_ptr<Placement> PersistenceWriter::getPlacement(std::string const& creator,
                                                           std::string const& label)
{
  auto const config_item = findConfigItem(m_output_items, label);

  if (!config_item) {
    throw std::runtime_error("No configuration found for product: " + label +
                             " from creator: " + creator);
  }

  std::string const full_label = buildFullLabel(creator, label);
  return std::make_unique<Placement>(config_item->file_name, full_label, config_item->technology);
}
