// Copyright (C) 2025 ...

#include "persistence.hpp"

#include <algorithm>
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
                                   std::map<std::string, std::string> const& products)
{
  std::map<std::unique_ptr<Placement>, std::string> containers;
  for (auto const& [label, type] : products) {
    containers.insert(std::make_pair(getPlacement(creator, label), type));
  }
  containers.insert(std::make_pair(getPlacement(creator, "index"), "std::string"));
  m_store->createContainers(containers, m_tech_settings);
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
  m_store->readContainer(*token, data, type, m_tech_settings);
  return;
}

std::unique_ptr<Placement> Persistence::getPlacement(std::string const& creator,
                                                     std::string const& label)
{
  //use config to determine values
  // Find exact match in config for regular data products
  auto const found = std::find_if(m_output_items.getItems().begin(),
                                  m_output_items.getItems().end(),
                                  [&label](auto const& persItem) {
                                    // Special handling for index containers, take first file, tech, FIXME: Reconsider
                                    return persItem.product_name == label || label == "index";
                                  });

  if (found == m_output_items.getItems().end()) {
    throw std::runtime_error("No configuration found for product: " + label +
                             " from creator: " + creator);
  }

  std::string full_label = creator + "/" + label;
  return std::make_unique<Placement>(found->file_name, full_label, found->technology);
}

std::unique_ptr<Token> Persistence::getToken(std::string const& creator,
                                             std::string const& label,
                                             std::string const& id)
{
  // Full label and index label construction
  std::string full_label = creator + "/" + label;
  std::string index_label = creator + "/index";

  // Get parameters from configuration
  auto const found =
    std::find_if(m_output_items.getItems().begin(),
                 m_output_items.getItems().end(),
                 [&label](auto const& persItem) { return persItem.product_name == label; });

  if (found == m_output_items.getItems().end()) {
    throw std::runtime_error("No configuration found for product: " + label +
                             " from creator: " + creator);
  }

  int const rowId =
    m_store->getIndex(Token{found->file_name, index_label, found->technology}, id, m_tech_settings);
  return std::make_unique<Token>(found->file_name, full_label, found->technology, rowId);
}
