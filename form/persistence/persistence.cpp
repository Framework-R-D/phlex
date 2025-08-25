// Copyright (C) 2025 ...

#include "persistence.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

using namespace form::detail::experimental;

// Factory function implementation
namespace form::detail::experimental {
  std::unique_ptr<IPersistence> createPersistence(
    form::experimental::config::parse_config const& config) //factory takes form config
  {
    return std::unique_ptr<IPersistence>(new Persistence(config));
  }
} // namespace form::detail::experimental

Persistence::Persistence(form::experimental::config::parse_config const& config) :
  m_store(createStorage()), m_config(config) // constructor takes form config
{
}

void Persistence::createContainers(std::string const& creator,
                                   std::map<std::string, std::string> const& products)
{
  std::map<std::unique_ptr<Placement>, std::string> containers;
  for (auto const& [label, type] : products) {
    containers.insert(std::make_pair(getPlacement(creator, label), type));
  }
  containers.insert(std::make_pair(getPlacement(creator, "index"), "std::string"));
  m_store->createContainers(containers);
  return;
}

void Persistence::registerWrite(std::string const& creator,
                                std::string const& label,
                                void const* data,
                                std::string const& type)
{
  std::unique_ptr<Placement> plcmnt = getPlacement(creator, label);
  m_store->fillContainer(*plcmnt, data, type);
  return;
}

void Persistence::commitOutput(std::string const& creator, std::string const& id)
{
  std::unique_ptr<Placement> plcmnt = getPlacement(creator, "index");
  m_store->fillContainer(*plcmnt, &id, "std::string");
  //FIXME: Flush Containers
  m_store->commitContainers(*plcmnt);
  return;
}

void Persistence::read(std::string const& creator,
                       std::string const& label,
                       std::string const& id,
                       void const** data,
                       std::string& type)
{
  std::unique_ptr<Token> token = getToken(creator, label, id);
  m_store->readContainer(*token, data, type);
  return;
}

form::experimental::config::PersistenceItem const* Persistence::findConfigItem(
  std::string const& label) const
{
  auto const& items = m_config.getItems();
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

  auto index_token =
    std::make_unique<Token>(config_item->file_name, index_label, config_item->technology);

  int rowId = m_store->getIndex(*index_token, id);

  return std::make_unique<Token>(
    config_item->file_name, full_label, config_item->technology, rowId);
}
