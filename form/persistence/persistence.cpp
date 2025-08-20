// Copyright (C) 2025 ...

#include "persistence.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

using namespace form::detail::experimental;

// Factory function implementation
namespace form::detail::experimental {
  std::unique_ptr<IPersistence> createPersistence(
    const form::experimental::config::parse_config& config) //factory takes form config
  {
    return std::unique_ptr<IPersistence>(new Persistence(config));
  }
} // namespace form::detail::experimental

Persistence::Persistence(const form::experimental::config::parse_config& config) :
  m_store(nullptr), m_config(config) // constructor takes form config
{
  m_store = createStorage();
}

void Persistence::createContainers(const std::string& creator,
                                   const std::map<std::string, std::string>& products)
{
  std::map<std::unique_ptr<Placement>, std::string> containers;
  for (const auto& [label, type] : products) {
    containers.insert(std::make_pair(getPlacement(creator, label), type));
  }
  containers.insert(std::make_pair(getPlacement(creator, "index"), "std::string"));
  m_store->createContainers(containers);
  return;
}

void Persistence::registerWrite(const std::string& creator,
                                const std::string& label,
                                const void* data,
                                const std::string& type)
{
  std::unique_ptr<Placement> plcmnt = getPlacement(creator, label);
  m_store->fillContainer(*plcmnt, data, type);
  return;
}

void Persistence::commitOutput(const std::string& creator, const std::string& id)
{
  std::unique_ptr<Placement> plcmnt = getPlacement(creator, "index");
  m_store->fillContainer(*plcmnt, &id, "std::string");
  //FIXME: Flush Containers
  m_store->commitContainers(*plcmnt);
  return;
}

void Persistence::read(const std::string& creator,
                       const std::string& label,
                       const std::string& id,
                       const void** data,
                       std::string& type)
{
  std::unique_ptr<Token> token = getToken(creator, label, id);
  m_store->readContainer(*token, data, type);
  return;
}

const form::experimental::config::PersistenceItem* Persistence::findConfigItem(
  const std::string& label) const
{
  const auto& items = m_config.getItems();
  auto it = std::find_if(
    items.begin(), items.end(), [&label](const auto& item) { return item.product_name == label; });

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

std::unique_ptr<Placement> Persistence::getPlacement(const std::string& creator,
                                                     const std::string& label)
{
  const auto* config_item = findConfigItem(label);

  if (!config_item) {
    throw std::runtime_error("No configuration found for product: " + label +
                             " from creator: " + creator);
  }

  const std::string full_label = buildFullLabel(creator, label);

  return std::make_unique<Placement>(config_item->file_name, full_label, config_item->technology);
}

std::unique_ptr<Token> Persistence::getToken(const std::string& creator,
                                             const std::string& label,
                                             const std::string& id)
{
  const auto* config_item = findConfigItem(label);

  if (!config_item) {
    throw std::runtime_error("No configuration found for product: " + label +
                             " from creator: " + creator);
  }

  const std::string full_label = buildFullLabel(creator, label);
  const std::string index_label = buildFullLabel(creator, "index");

  auto index_token =
    std::make_unique<Token>(config_item->file_name, index_label, config_item->technology);

  int rowId = m_store->getIndex(*index_token, id);

  return std::make_unique<Token>(
    config_item->file_name, full_label, config_item->technology, rowId);
}
