// Copyright (C) 2025 ...

#include "persistence.hpp"

#include <cstring>
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
  const form::experimental::config::tech_setting_config& tech_config_settings)
{
  m_tech_settings = tech_config_settings;
}

void Persistence::configureOutputItems(
  const form::experimental::config::output_item_config& output_items)
{
  m_output_items = output_items;
}

void Persistence::createContainers(const std::string& creator,
                                   const std::map<std::string, std::string>& products)
{
  std::map<std::unique_ptr<Placement>, std::string> containers;
  for (const auto& [label, type] : products) {
    containers.insert(std::make_pair(getPlacement(creator, label), type));
  }
  containers.insert(std::make_pair(getPlacement(creator, "index"), "std::string"));
  m_store->createContainers(containers, m_tech_settings);
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
  m_store->readContainer(*token, data, type, m_tech_settings);
  return;
}

std::unique_ptr<Placement> Persistence::getPlacement(const std::string& creator,
                                                     const std::string& label)
{
  //use config to determine values
  std::string file_name;
  int technology;

  bool found = false;
  // Find exact match in config for regular data products
  for (const auto& item : m_output_items.getItems()) {
    // Special handling for index containers, take first file, tech, FIXME: Reconsider
    if (item.product_name == label || label == "index") {
      file_name = item.file_name;
      technology = item.technology;
      found = true;
      break;
    }
  }
  if (!found) {
    throw std::runtime_error("No configuration found for product: " + label +
                             " from creator: " + creator);
  }

  std::string full_label = creator + "/" + label;
  std::unique_ptr<Placement> plcmnt =
    std::unique_ptr<Placement>(new Placement(file_name, full_label, technology));
  return plcmnt;
}

std::unique_ptr<Token> Persistence::getToken(const std::string& creator,
                                             const std::string& label,
                                             const std::string& id)
{
  // Full label and index label construction
  std::string full_label = creator + "/" + label;
  std::string index_label = creator + "/index";

  // Get parameters from configuration
  std::string file_name;
  int technology;
  bool found = false;
  for (const auto& item : m_output_items.getItems()) {
    if (item.product_name == label) {
      file_name = item.file_name;
      technology = item.technology;
      found = true;
      break;
    }
  }

  if (!found) {
    throw std::runtime_error("No configuration found for product: " + label +
                             " from creator: " + creator);
  }

  std::unique_ptr<Token> index_token =
    std::unique_ptr<Token>(new Token(file_name, index_label, technology));
  int rowId = m_store->getIndex(*index_token, id, m_tech_settings);
  std::unique_ptr<Token> token =
    std::unique_ptr<Token>(new Token(file_name, full_label, technology, rowId));
  return token;
}
