// Copyright (C) 2025 ...

#include "persistence_reader.hpp"
#include "persistence_utils.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>

using namespace form::detail::experimental;

namespace form::detail::experimental {
  std::unique_ptr<IPersistenceReader> createPersistenceReader()
  {
    return std::make_unique<PersistenceReader>();
  }
}

PersistenceReader::PersistenceReader() :
  m_store_reader(createStorageReader()),
  m_output_items(),
  m_tech_settings() // constructor takes form config
{
}

void PersistenceReader::configureTechSettings(
  form::experimental::config::tech_setting_config const& tech_config_settings)
{
  m_tech_settings = tech_config_settings;
}

void PersistenceReader::configureOutputItems(
  form::experimental::config::ItemConfig const& output_items)
{
  m_output_items = output_items;
}

void PersistenceReader::read(std::string const& creator,
                             std::string const& label,
                             std::string const& id,
                             void const** data,
                             std::type_info const& type)
{
  std::unique_ptr<Token> token = getToken(creator, label, id);
  m_store_reader->readContainer(*token, data, type, m_tech_settings);
  return;
}

std::unique_ptr<Token> PersistenceReader::getToken(std::string const& creator,
                                                   std::string const& label,
                                                   std::string const& id)
{
  auto const config_item = findConfigItem(m_output_items, label);

  if (!config_item) {
    throw std::runtime_error("No configuration found for product: " + label +
                             " from creator: " + creator);
  }

  std::string const full_label = buildFullLabel(creator, label);
  std::string const index_label = buildFullLabel(creator, "index");

  int const rowId = m_store_reader->getIndex(
    Token{config_item->file_name, index_label, config_item->technology}, id, m_tech_settings);
  return std::make_unique<Token>(
    config_item->file_name, full_label, config_item->technology, rowId);
}
