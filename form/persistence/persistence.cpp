// Copyright (C) 2025 ...

#include "persistence.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>

using namespace form::detail::experimental;

// Factory function implementation
namespace form::detail::experimental {
  std::unique_ptr<IPersistence> createPersistence() { return std::make_unique<Persistence>(); }
} // namespace form::detail::experimental

Persistence::Persistence() :
  m_store(createStorage()), m_output_items(), m_tech_settings() // constructor takes form config
{
}

void Persistence::configureTechSettings(
  form::experimental::config::tech_setting_config const& tech_config_settings)
{
  m_tech_settings = tech_config_settings;
}

void Persistence::configureOutputItems(
  form::experimental::config::output_item_config const& output_items)
{
  m_output_items = output_items;
}

void Persistence::createContainers(std::string const& creator,
                                   std::map<std::string, std::type_info const*> const& products)
{
  std::map<std::unique_ptr<Placement>, std::type_info const*> containers;
  for (auto const& [label, type] : products) {
    containers.insert(std::make_pair(getPlacement(creator, label), type));
  }
  containers.insert(std::make_pair(getPlacement(creator, "index"), &typeid(std::string)));
  m_store->createContainers(containers, m_tech_settings);
  return;
}

void Persistence::registerWrite(std::string const& creator,
                                std::string const& label,
                                void const* data,
                                std::type_info const& type)
{
  std::unique_ptr<Placement> plcmnt = getPlacement(creator, label);
  m_store->fillContainer(*plcmnt, data, type);
  return;
}

void Persistence::commitOutput(std::string const& creator, std::string const& id)
{
  std::unique_ptr<Placement> plcmnt = getPlacement(creator, "index");
  m_store->fillContainer(*plcmnt, &id, typeid(std::string));
  m_store->commitContainers(*plcmnt);
  return;
}

void Persistence::read(std::string const& creator,
                       std::string const& label,
                       std::string const& id,
                       void const** data,
                       const std::type_info& type)
{
  std::unique_ptr<Token> token = getToken(creator, label, id);
  m_store->readContainer(*token, data, type, m_tech_settings);
  return;
}

form::experimental::config::PersistenceItem const* Persistence::findConfigItem(
  std::string const& label) const
{
  auto const& items = m_output_items.getItems();
  if (label == "index")
    return (items.empty())
             ? nullptr
             : &(*items
                    .begin()); //emulate how FORM did this before Phlex PR #22.  Will be fixed in a future FORM update.

  auto it = std::find_if(
    items.begin(), items.end(), [&label](auto const& item) { return item.product_name == label; });

  return (it != items.end()) ? &(*it) : nullptr;
}

std::string Persistence::buildFullLabel(std::string_view creator, std::string_view label) const
{
  std::string result;
  result.reserve(creator.size() + 1 + label.size());
  result += creator;
  result += '/';
  result += label;
  return result;
}

std::unique_ptr<Placement> Persistence::getPlacement(std::string const& creator,
                                                     std::string const& label)
{
  auto const* config_item = findConfigItem(label);

  if (!config_item) {
    throw std::runtime_error("No configuration found for product: " + label +
                             " from creator: " + creator);
  }

  std::string const full_label = buildFullLabel(creator, label);
  return std::make_unique<Placement>(config_item->file_name, full_label, config_item->technology);
}

std::unique_ptr<Token> Persistence::getToken(std::string const& creator,
                                             std::string const& label,
                                             std::string const& id)
{
  auto const* config_item = findConfigItem(label);

  if (!config_item) {
    throw std::runtime_error("No configuration found for product: " + label +
                             " from creator: " + creator);
  }

  std::string const full_label = buildFullLabel(creator, label);
  std::string const index_label = buildFullLabel(creator, "index");

  int const rowId = m_store->getIndex(
    Token{config_item->file_name, index_label, config_item->technology}, id, m_tech_settings);
  return std::make_unique<Token>(
    config_item->file_name, full_label, config_item->technology, rowId);
}
